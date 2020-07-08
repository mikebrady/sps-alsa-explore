// ALSAExplore is based on amixer, license below.

/*
 *   ALSA command line mixer utility
 *   Copyright (c) 1999-2000 by Jaroslav Kysela <perex@perex.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "alsaexplore.h"
#include "config.h"
#include <alsa/asoundlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "../alsamixer/volume_mapping.h"

#define LEVEL_BASIC (1 << 0)
#define LEVEL_INACTIVE (1 << 1)
#define LEVEL_ID (1 << 2)

char card[64];

void error(const char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  fprintf(stderr, "%s: ", PACKAGE_NAME);
  vfprintf(stderr, fmt, va);
  fprintf(stderr, "\n");
  va_end(va);
}

static int selems_if_has_db_playback(int include_mixers_with_capture, int quiet) {
  int result = 0;
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);
  long min_db, max_db;

  if ((result = snd_mixer_open(&handle, 0)) < 0) {
    if (quiet == 0)
      error("Mixer %s open error: %s", card, snd_strerror(result));
  } else {
    if ((result = snd_mixer_attach(handle, card)) < 0) {
      if (quiet == 0)
        error("Mixer attach %s error: %s", card, snd_strerror(result));
    } else {
      if ((result = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
        if (quiet == 0)
          error("Mixer register error: %s", snd_strerror(result));
      } else {
        if ((result = snd_mixer_load(handle)) < 0) {
          if (quiet == 0)
            error("Mixer %s load error: %s", card, snd_strerror(result));
        } else {
          for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem)) {
              int has_capture_elements =
                  snd_mixer_selem_has_common_volume(elem) || snd_mixer_selem_has_capture_volume(elem) ||
                  snd_mixer_selem_has_common_switch(elem) || snd_mixer_selem_has_capture_switch(elem);

              if (((include_mixers_with_capture == 1) && has_capture_elements) ||
                  ((include_mixers_with_capture == 0) && (!has_capture_elements))) {
                if (snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db) == 0) {
                  snd_mixer_selem_get_id(elem, sid);
                  if (quiet == 0)
                    printf("                 \"%s\"\n", snd_mixer_selem_id_get_name(sid));
                  result++;
                }
              }
            }
          }
        }
      }
    }
    snd_mixer_close(handle);
  }
  return result;
}

typedef enum {
  SPS_FORMAT_UNKNOWN = 0,
  SPS_FORMAT_S8,
  SPS_FORMAT_U8,
  SPS_FORMAT_S16,
  SPS_FORMAT_S16_LE,
  SPS_FORMAT_S16_BE,
  SPS_FORMAT_S24,
  SPS_FORMAT_S24_LE,
  SPS_FORMAT_S24_BE,
  SPS_FORMAT_S24_3LE,
  SPS_FORMAT_S24_3BE,
  SPS_FORMAT_S32,
  SPS_FORMAT_S32_LE,
  SPS_FORMAT_S32_BE,
  SPS_FORMAT_AUTO,
  SPS_FORMAT_INVALID,
} sps_format_t;

snd_pcm_t *alsa_handle = NULL;
snd_pcm_hw_params_t *alsa_params = NULL;
snd_pcm_sw_params_t *alsa_swparams = NULL;
int frame_size; // in bytes for interleaved stereo


// This array is a sequence of the output rates to be tried if automatic speed selection is
// requested.
// There is no benefit to upconverting the frame rate, other than for compatibility.
// The lowest rate that the DAC is capable of is chosen.

unsigned int auto_speed_output_rates[] = {
    44100,
    88200,
    176400,
    352800,
};

// This array is of all the formats known to Shairport Sync, in order of the SPS_FORMAT definitions,
// with their equivalent alsa codes and their frame sizes.
// If just one format is requested, then its entry is searched for in the array and checked on the
// device
// If auto format is requested, then each entry in turn is tried until a working format is found.
// So, it should be in the search order.

typedef struct {
  snd_pcm_format_t alsa_code;
  int frame_size;
} format_record;

format_record fr[] = {
    {SND_PCM_FORMAT_UNKNOWN, 0}, // unknown
    {SND_PCM_FORMAT_S8, 2},      {SND_PCM_FORMAT_U8, 2},      {SND_PCM_FORMAT_S16, 4},
    {SND_PCM_FORMAT_S16_LE, 4},  {SND_PCM_FORMAT_S16_BE, 4},  {SND_PCM_FORMAT_S24, 8},
    {SND_PCM_FORMAT_S24_LE, 8},  {SND_PCM_FORMAT_S24_BE, 8},  {SND_PCM_FORMAT_S24_3LE, 6},
    {SND_PCM_FORMAT_S24_3BE, 6}, {SND_PCM_FORMAT_S32, 8},     {SND_PCM_FORMAT_S32_LE, 8},
    {SND_PCM_FORMAT_S32_BE, 8},  {SND_PCM_FORMAT_UNKNOWN, 0}, // auto
    {SND_PCM_FORMAT_UNKNOWN, 0},                              // illegal
};

// This array is the sequence of formats to be tried if automatic selection of the format is
// requested.
// Ideally, audio should pass through Shairport Sync unaltered, apart from occasional interpolation.
// If the user chooses a hardware mixer, then audio could go straight through, unaltered, as signed
// 16 bit stereo.
// However, the user might, at any point, select an option that requires modification, such as
// stereo to mono mixing,
// additional volume attenuation, convolution, and so on. For this reason,
// we look for the greatest depth the DAC is capable of, since upconverting it is completely
// lossless.
// If audio processing is required, then the dither that must be added will
// be added at the lowest possible level.
// Hence, selecting the greatest bit depth is always either beneficial or neutral.

sps_format_t auto_format_check_sequence[] = {
    SPS_FORMAT_S32,    SPS_FORMAT_S32_LE,  SPS_FORMAT_S32_BE,  SPS_FORMAT_S24, SPS_FORMAT_S24_LE,
    SPS_FORMAT_S24_BE, SPS_FORMAT_S24_3LE, SPS_FORMAT_S24_3BE, SPS_FORMAT_S16, SPS_FORMAT_S16_LE,
    SPS_FORMAT_S16_BE, SPS_FORMAT_S8,      SPS_FORMAT_U8,
};

const char *sps_format_description_string_array[] = {
    "unknown", "S8",      "U8",      "S16", "S16_LE", "S16_BE", "S24",  "S24_LE",
    "S24_BE",  "S24_3LE", "S24_3BE", "S32", "S32_LE", "S32_BE", "auto", "invalid"};

const char *sps_format_description_string(sps_format_t format) {
  if (format <= SPS_FORMAT_AUTO)
    return sps_format_description_string_array[format];
  else
    return sps_format_description_string_array[SPS_FORMAT_INVALID];
}


int check_alsa_device(int quiet) {
  int ret, dir = 0;
  unsigned int actual_sample_rate; // this will be given the rate requested and will be given the actual rate

  ret = snd_pcm_open(&alsa_handle, card, SND_PCM_STREAM_PLAYBACK, 0);
  if (ret < 0) {
    if (ret == -ENOENT) {
      if (quiet == 0)
        error("the alsa output_device \"%s\" can not be found.", card);
    } else if (quiet == 0) {
      char errorstring[1024];
      strerror_r(-ret, (char *)errorstring, sizeof(errorstring));
      error("error %d (\"%s\") opening alsa device \"%s\".", ret, (char *)errorstring,
           card);
    }
    return -1; // alsa handle not allocated so we're okay
  }

  snd_pcm_hw_params_alloca(&alsa_params);
  snd_pcm_sw_params_alloca(&alsa_swparams);

  ret = snd_pcm_hw_params_any(alsa_handle, alsa_params);
  if (ret < 0) {
    if (quiet == 0)
      error("broken configuration for device \"%s\": no configurations "
        "available",
        card);
    return -1;
  }

  if ((snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED) <
       0) && (snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_MMAP_INTERLEAVED) >=
       0)) {
    if (quiet == 0)
      error("interleaved access not available for device \"%s\": %s", card,
         snd_strerror(ret));
    return -1;
  }

  ret = snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, 2);
  if (ret < 0) {
    if (quiet == 0)
      error("stereo output not available for device \"%s\": %s", card,
         snd_strerror(ret));
    return -1;
  }

  snd_pcm_format_t sf;
  int number_of_formats_to_try;
  sps_format_t *formats;
  formats = auto_format_check_sequence;
  number_of_formats_to_try = sizeof(auto_format_check_sequence) / sizeof(sps_format_t);
  int i = 0;
  int format_found = 0;
  sps_format_t trial_format = SPS_FORMAT_UNKNOWN;
  while ((i < number_of_formats_to_try) && (format_found == 0)) {
    trial_format = formats[i];
    sf = fr[trial_format].alsa_code;
    frame_size = fr[trial_format].frame_size;
    ret = snd_pcm_hw_params_set_format(alsa_handle, alsa_params, sf);
    if (ret == 0)
      format_found = 1;
    else
      i++;
  }
  if (ret == 0) {
  } else {
    if (quiet == 0)
      error("could not find an output format for device \"%s\": %s",
         card, snd_strerror(ret));
    return -1;
  }

  int number_of_speeds_to_try;
  unsigned int *speeds;

  speeds = auto_speed_output_rates;
  number_of_speeds_to_try = sizeof(auto_speed_output_rates) / sizeof(int);

  i = 0;
  int speed_found = 0;

  while ((i < number_of_speeds_to_try) && (speed_found == 0)) {
    actual_sample_rate = speeds[i];
    ret = snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, &actual_sample_rate, &dir);
    if (ret == 0) {
      speed_found = 1;
    } else {
      i++;
    }
  }
  if (ret == 0) {
    // error("alsaexplore: output speed found is %d.", actual_sample_rate);
  } else {
    if (quiet == 0)
      error("could not find a suitable output rate for device \"%s\": %s",
         card, snd_strerror(ret));
    return -1;
  }

  if (quiet == 0)
    printf("Automatic Output Format:  \"%s/%d\".\n",
          sps_format_description_string(trial_format), actual_sample_rate);


  ret = snd_pcm_hw_params(alsa_handle, alsa_params);
  if (ret < 0) {
    if (quiet == 0)
      error("unable to set hardware parameters for device \"%s\": %s.", card,
         snd_strerror(ret));
    return -1;
  }

  ret = snd_pcm_sw_params_current(alsa_handle, alsa_swparams);
  if (ret < 0) {
    if (quiet == 0)
      error("unable to get software parameters for device \"%s\": "
         "%s.",
         card, snd_strerror(ret));
    return -1;
  }

  ret = snd_pcm_sw_params_set_tstamp_mode(alsa_handle, alsa_swparams, SND_PCM_TSTAMP_ENABLE);
  if (ret < 0) {
    if (quiet == 0)
      error("can not enable timestamp mode of device: \"%s\": %s.", card,
         snd_strerror(ret));
    return -1;
  }

  /* write the sw parameters */
  ret = snd_pcm_sw_params(alsa_handle, alsa_swparams);
  if (ret < 0) {
    if (quiet == 0)
      error("unable to set software parameters of device: \"%s\": %s.", card,
         snd_strerror(ret));
    return -1;
  }
  return 0;
}


static int cards(void) {
  snd_ctl_t *handle;
  int card_number, err, dev;
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  snd_ctl_card_info_alloca(&info);
  snd_pcm_info_alloca(&pcminfo);

  card_number = -1;
  if (snd_card_next(&card_number) < 0 || card_number < 0) {
    error("no soundcards found...");
    return -1;
  }
  while (card_number >= 0) {
    sprintf(card, "hw:%d", card_number);
    if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
      error("control open (%i): %s", card_number, snd_strerror(err));
      goto next_card;
    }
    if ((err = snd_ctl_card_info(handle, info)) < 0) {
      error("control hardware info (%i): %s", card_number, snd_strerror(err));
      snd_ctl_close(handle);
      goto next_card;
    }
    dev = -1;
    while (1) {
      if (snd_ctl_pcm_next_device(handle, &dev) < 0)
        error("snd_ctl_pcm_next_device");
      if (dev < 0)
        break;
      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
      if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
        if (err != -ENOENT)
          error("control digital audio info (%i): %s", card, snd_strerror(err));
        continue;
      }
      printf("ALSA Hardware Output Device %d:\n", card_number);
      if (check_alsa_device(1) == 0) {
        printf("Full Name:       \"hw:CARD=%s,DEV=%i\"\n", snd_ctl_card_info_get_id(info), dev);
        printf("Short Name:      ");
        if (dev > 0)
          printf("\"hw:%i,%i\"\n",card_number, dev);
        else
          printf("\"hw:%i\"\n",card_number);
        if ((selems_if_has_db_playback(0,1)) || (selems_if_has_db_playback(1,1))) {
          printf("dB Mixers:\n");
          selems_if_has_db_playback(0,0); // omit mixers that also have a capture part
          selems_if_has_db_playback(1,0); // include mixers that also have a capture part
        } else {
          printf("No Mixers.\n");
        }
      } else {
        printf("Shairport Sync can not use this device.\n");
      }


      // printf("Output Device hw:%i: %s [%s], device %i: %s [%s]\n", card_number, snd_ctl_card_info_get_id(info),
      //       snd_ctl_card_info_get_name(info), dev, snd_pcm_info_get_id(pcminfo),
      //       snd_pcm_info_get_name(pcminfo));
      /*
      count = snd_pcm_info_get_subdevices_count(pcminfo);
      printf("  Subdevices: %i/%i\n", snd_pcm_info_get_subdevices_avail(pcminfo), count);
      for (idx = 0; idx < (int)count; idx++) {
              snd_pcm_info_set_subdevice(pcminfo, idx);
              if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                      error("control digital audio playback info (%i): %s", card,
      snd_strerror(err)); } else { printf("  Subdevice #%i: %s\n", idx,
      snd_pcm_info_get_subdevice_name(pcminfo));
              }
      }
      */
      printf("\n");
    }
    snd_ctl_close(handle);
  next_card:
    if (snd_card_next(&card_number) < 0) {
      error("snd_card_next");
      break;
    }
  }
  return 0;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    return cards() ? 1 : 0;
}
