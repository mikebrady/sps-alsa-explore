// SPS-ALSA-Explore is based, with thanks, on amixer v1.0.3, license below.

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

// extended and modified (C) 2021-2024 by Mike Brady <4265913+mikebrady@users.noreply.github.com>

#include "sps-alsa-explore.h"
#include "gitversion.h"
#include <alsa/asoundlib.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <getopt.h>
#include <grp.h>
#include <math.h>
#include <poll.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LEVEL_BASIC (1 << 0)
#define LEVEL_INACTIVE (1 << 1)
#define LEVEL_ID (1 << 2)

char card[64];
int extended_output = 0;
int check_subdevices = 0;

static int selems_if_has_db_playback(int include_mixers_with_capture, char *firstPrompt,
                                     char *subsequentPrompt) {
  int result = 0;
  int firstPromptUsed = 0;
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);
  long min_db, max_db;

  if ((result = snd_mixer_open(&handle, 0)) < 0) {
    if (firstPrompt != NULL)
      debug(1, "Mixer %s open error: %s", card, snd_strerror(result));
  } else {
    if ((result = snd_mixer_attach(handle, card)) < 0) {
      if (firstPrompt != NULL)
        debug(1, "Mixer attach %s error: %s", card, snd_strerror(result));
    } else {
      if ((result = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
        if (firstPrompt != NULL)
          debug(1, "Mixer register error: %s", snd_strerror(result));
      } else {
        if ((result = snd_mixer_load(handle)) < 0) {
          if (firstPrompt != NULL)
            debug(1, "Mixer %s load error: %s", card, snd_strerror(result));
        } else {
          for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem)) {
              int has_capture_elements = snd_mixer_selem_has_common_volume(elem) ||
                                         snd_mixer_selem_has_capture_volume(elem) ||
                                         snd_mixer_selem_has_common_switch(elem) ||
                                         snd_mixer_selem_has_capture_switch(elem);

              if (((include_mixers_with_capture == 1) && has_capture_elements) ||
                  ((include_mixers_with_capture == 0) && (!has_capture_elements))) {
                if (snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db) == 0) {
                  if (min_db == SND_CTL_TLV_DB_GAIN_MUTE) {
                    // For instance, the Raspberry Pi does this
                    debug(1, "Lowest dB value is a mute");
                    long minv = 0;
                    long maxv = 0;
                    if (snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv) < 0)
                      debug(1, "Can't read mixer's [linear] min and max volumes.");

                    if (snd_mixer_selem_ask_playback_vol_dB(elem, minv + 1, &min_db) != 0)
                      debug(1, "Can't get dB value corresponding to a minimum volume "
                               "+ 1.");
                  }
                  if (firstPrompt != NULL) {

                    if (extended_output == 0)
                      inform("%s\"%s\",%d%*sRange: %6.2f dB",
                             (firstPromptUsed != 0) && (subsequentPrompt != NULL) ? subsequentPrompt
                                                                                  : firstPrompt,
                             snd_mixer_selem_get_name(elem), snd_mixer_selem_get_index(elem),
                             20 - strlen(snd_mixer_selem_get_name(elem)), " ",
                             (max_db - min_db) * 0.01);
                    else
                      inform("%s\"%s\",%d%*sRange: %6.2f dB, max: %6.2f dB, min: %6.2f dB",
                             (firstPromptUsed != 0) && (subsequentPrompt != NULL) ? subsequentPrompt
                                                                                  : firstPrompt,
                             snd_mixer_selem_get_name(elem), snd_mixer_selem_get_index(elem),
                             20 - strlen(snd_mixer_selem_get_name(elem)), " ",
                             (max_db - min_db) * 0.01, max_db * 0.01, min_db * 0.01);
                    firstPromptUsed = 1;
                  }
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

unsigned int alternate_speed_output_rates[] = {
    8000, 48000, 96000, 192000, 384000,
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

sps_format_t format_check_sequence[] = {
    SPS_FORMAT_S32_LE,  SPS_FORMAT_S32_BE, SPS_FORMAT_S24_LE, SPS_FORMAT_S24_BE, SPS_FORMAT_S24_3LE,
    SPS_FORMAT_S24_3BE, SPS_FORMAT_S16_LE, SPS_FORMAT_S16_BE, SPS_FORMAT_S8,     SPS_FORMAT_U8,
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

int check_alsa_device_with_settings(const char *device, snd_pcm_format_t sample_format,
                                    unsigned int sample_rate) {

  // returns 0 if successful, -2 if can't set format, -3 if can't set speed
  // -4 if device is busy, -5 if device can't be opened -1 otherwise
  int result = -1;
  int ret, dir = 0;
  ret = snd_pcm_open(&alsa_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
  if (ret == 0) {
    snd_pcm_hw_params_alloca(&alsa_params);
    snd_pcm_sw_params_alloca(&alsa_swparams);
    ret = snd_pcm_hw_params_any(alsa_handle, alsa_params);
    if (ret == 0) {

      if ((snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED) ==
           0) ||
          (snd_pcm_hw_params_set_access(alsa_handle, alsa_params,
                                        SND_PCM_ACCESS_MMAP_INTERLEAVED) == 0)) {
        ret = snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, 2);
        if (ret == 0) {
          ret = snd_pcm_hw_params_set_format(alsa_handle, alsa_params, sample_format);
          if (ret == 0) {
            unsigned int actual_sample_rate = sample_rate;
            ret = snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, &actual_sample_rate,
                                                  &dir);
            if ((ret == 0) && (actual_sample_rate != sample_rate))
              debug(2, "Sample rate set, %u, is different to sample rate requested, %u.",
                    actual_sample_rate, sample_rate);
            if ((ret == 0) && (actual_sample_rate == sample_rate)) {
              ret = snd_pcm_hw_params(alsa_handle, alsa_params);
              if (ret == 0) {
                ret = snd_pcm_sw_params_current(alsa_handle, alsa_swparams);
                if (ret == 0) {
                  ret = snd_pcm_sw_params_set_tstamp_mode(alsa_handle, alsa_swparams,
                                                          SND_PCM_TSTAMP_ENABLE);
                  if (ret == 0) {
                    /* write the sw parameters */
                    ret = snd_pcm_sw_params(alsa_handle, alsa_swparams);
                    if (ret == 0) {
                      result = 0; // success
                    } else {
                      debug(1, "unable to set software parameters of device: \"%s\": %s.", card,
                            snd_strerror(ret));
                    }
                  } else {
                    debug(1, "can not enable timestamp mode of device: \"%s\": %s.", card,
                          snd_strerror(ret));
                  }
                } else {

                  debug(1,
                        "unable to get software parameters for device \"%s\": "
                        "%s.",
                        card, snd_strerror(ret));
                }
              } else {
                debug(1, "unable to set hardware parameters for device \"%s\": %s.", card,
                      snd_strerror(ret));
                result =
                    -6; // -6 means the device finally complained when writing the hardware settings
              }
            } else {
              debug(2, "could not set output rate %u for device \"%s\": %s", actual_sample_rate,
                    card, snd_strerror(ret));
              result = -3; // -3 means can't set rate
            }
          } else {
            debug(2, "could not set output format %d for device \"%s\": %s", sample_format, card,
                  snd_strerror(ret));
            result = -2; // -3 means can't set format
          }
        } else {
          debug(1, "stereo output not available for device \"%s\": %s", card, snd_strerror(ret));
        }
      } else {
        debug(1, "interleaved access not available for device \"%s\": %s", card, snd_strerror(ret));
      }
    } else {
      debug(1,
            "broken configuration for device \"%s\": no configurations "
            "available",
            card);
    }
    // now close the device
    snd_pcm_close(alsa_handle);
  } else {
    if (ret == -ENODEV) {
      debug(1, "the alsa output_device \"%s\" can not be opened.", device);
      result = -5;
    } else if (ret == -EBUSY) {
      result = -4;
      debug(1, "the alsa output_device \"%s\" is busy.", device);
    } else {
      char errorstring[1024];
      strerror_r(-ret, (char *)errorstring, sizeof(errorstring));
      debug(1, "error %d (\"%s\") opening alsa device \"%s\".", ret, (char *)errorstring, device);
    }
  }
  return result;
}

int check_alsa_device(const char *device, int quiet, int stop_on_first_success,
                      int check_alternate_speeds) {
  int response = 0;
  int number_of_formats_to_try = sizeof(format_check_sequence) / sizeof(sps_format_t);
  int number_of_speeds_to_try = sizeof(auto_speed_output_rates) / sizeof(int);
  unsigned int *speeds = auto_speed_output_rates;
  if (check_alternate_speeds != 0) {
    number_of_speeds_to_try = sizeof(alternate_speed_output_rates) / sizeof(int);
    speeds = alternate_speed_output_rates;
  }

  int ret;
  int i = 0;
  // pick speeds
  do {
    // pick next speed to check
    unsigned int sample_rate = speeds[i];
    int number_of_successes = 0;
    char information_string[1024];
    information_string[0] = '\0';
    snprintf(information_string, sizeof(information_string) - 1 - strlen(information_string),
             "     %-6u", sample_rate);
    // pick formats
    int j = 0;
    do {
      // pick next format to check
      snd_pcm_format_t sample_format = fr[format_check_sequence[j]].alsa_code;
      const char *desc = sps_format_description_string_array[format_check_sequence[j]];
      // debug(1, "check %d, %s", sample_rate, desc );
      ret = check_alsa_device_with_settings(device, sample_format, sample_rate);
      debug(2, "check %d, %s, result: %d.", sample_rate, desc, ret);
      // -2 and -3 mean the speed or format was not suitable and -6 means some setting was not
      // acceptable
      if ((ret != 0) && (ret != -2) && (ret != -3) &&
          (ret != -6)) // errors 2, 3 and 6 relate to an individual rejected setting
        response = ret;
      j++;
      if (ret == 0) {
        if (number_of_successes == 0)
          snprintf(information_string + strlen(information_string),
                   sizeof(information_string) - 1 - strlen(information_string), "            %s",
                   desc);
        else
          snprintf(information_string + strlen(information_string),
                   sizeof(information_string) - 1 - strlen(information_string), ",%s", desc);
        number_of_successes++;
        response++;
      }
    } while ((j < number_of_formats_to_try) && (response >= 0) &&
             (!((stop_on_first_success != 0) && (response == 1))));
    if ((number_of_successes > 0) && (quiet == 0))
      inform(information_string);
    if ((ret != 0) && (ret != -2) && (ret != -3) &&
        (ret != -6)) // errors 2, 3 and 6 are ignored as they are transient
      response = ret;
    i++;
  } while ((i < number_of_speeds_to_try) && (response >= 0) &&
           (!((stop_on_first_success != 0) && (response == 1))));

  return response; // -1 if a problem arose, -4 or -5 if busy or inaccessible, number of successes
                   // otherwise
}

static int cards(void) {
  int response = 0;
  snd_ctl_t *handle;
  int card_number, err, dev;
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  snd_ctl_card_info_alloca(&info);
  snd_pcm_info_alloca(&pcminfo);

  card_number = -1;
  snd_card_next(&card_number);

  // if (snd_card_next(&card_number) < 0 || card_number < 0) {
  //  debug(1, "no soundcards found...");
  //}
  while (card_number >= 0) {

    void **hints;

    if (snd_device_name_hint(card_number, "pcm", &hints) == 0) {
      debug(1, "device %d name hints received.", card_number);

      void **device_on_card_hints = hints;

      while (*device_on_card_hints != NULL) {
        char *device_on_card_name;
        device_on_card_name = snd_device_name_get_hint(*device_on_card_hints, "NAME");
        debug(1, "device_on_card_name is \"%s\".", device_on_card_name);
        /*
        descr = snd_device_name_get_hint(*n, "DESC");
        io = snd_device_name_get_hint(*n, "IOID");

        if (io != NULL) {
          debug(1, "io: \"%s\".", io);
                free(io);
        }
        debug(1,"--");
        */

        char device_type[64];
        strncpy(device_type, device_on_card_name, sizeof(device_type) - 1);
        char *p = device_type;
        while ((*p != ':') && (*p != '\0')) {
          p++;
        }
        *p = '\0';

        debug(1, "device_type: \"%s\".", device_type);

        // only look at hw: and hdmi: device types

        sprintf(card, "hw:%d", card_number);
        // strncpy(card, device_on_card_name, sizeof(card) - 1);
        if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
          debug(1, "control open of \"%s\" error: %s", card, snd_strerror(err));
          goto next_device_name_on_card;
        }
        if ((err = snd_ctl_card_info(handle, info)) < 0) {
          debug(1, "control hardware info (%i): %s", card_number, snd_strerror(err));
          snd_ctl_close(handle);
          goto next_device_name_on_card;
        }
        dev = -1;
        while (1) {
          if (snd_ctl_pcm_next_device(handle, &dev) < 0)
            debug(1, "snd_ctl_pcm_next_device");
          if (dev < 0)
            break;
          debug(1, "card number %d, device number: %d.", card_number, dev);
          snd_pcm_info_set_device(pcminfo, dev);
          snd_pcm_info_set_subdevice(pcminfo, 0);
          if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
            debug(1, "card %i, subdevice %i): %s", card_number, dev, snd_strerror(err));
            continue;
          }
          int sub_device_count = snd_pcm_info_get_subdevices_count(pcminfo);
          debug(1, "card %i has %d subdevices,", card_number, sub_device_count);

          int sub_device = 0;
          if ((strcmp(device_type, "hw") == 0) || (strcmp(device_type, "hdmi") == 0))
            do {
              snd_pcm_info_set_subdevice(pcminfo, sub_device);
              snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
              if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                if (err != -ENOENT)
                  debug(1, "snd_ctl_pcm_info error for card %i, subdevice %i: %s", card_number,
                        sub_device, snd_strerror(err));
                continue;
              }
              char device_name[128];
              char short_name[128];
              if ((sub_device_count <= 1) || (check_subdevices == 0)) {
                if (dev == 0) {
                  sprintf(device_name, "%s:%s", device_type, snd_ctl_card_info_get_id(info));
                  sprintf(short_name, "%s:%i", device_type, card_number);
                } else {
                  sprintf(device_name, "%s:CARD=%s,DEV=%i", device_type,
                          snd_ctl_card_info_get_id(info), dev);
                  sprintf(short_name, "%s:%i,%i", device_type, card_number, dev);
                }
              } else {
                sprintf(device_name, "%s:CARD=%s,DEV=%i,SUBDEV=%i", device_type,
                        snd_ctl_card_info_get_id(info), dev, sub_device);
                sprintf(short_name, "%s:%i,%i,%i", device_type, card_number, dev, sub_device);
              }
              debug(1, "device name: \"%s\"", device_name);
              if (check_subdevices == 0)
                debug(1, "card: %d, device: %d", card_number, dev);
              else
                debug(1, "card: %d, device: %d, sub_device: %d", card_number, dev, sub_device);

              if ((check_alsa_device(device_name, 1, 0, 0) >= 0) || (extended_output != 0) ||
                  (check_alsa_device(device_name, 1, 0, 0) == -4) ||
                  (check_alsa_device(device_name, 1, 0, 0) == -5)) {
                inform("> Device Full Name:    \"%s\"", device_name);
                inform("  Short Name:          \"%s\"", short_name);
                if ((sub_device_count > 1) && (check_subdevices == 0) && (extended_output))
                  inform("  Subdevices:           %i", sub_device_count);
                if (extended_output != 0) {
                  inform("    Card Name:         \"%s\"", snd_ctl_card_info_get_name(info));
                  inform("    Device ID:         \"%s\"", snd_pcm_info_get_id(pcminfo));
                  inform("    Device Name:       \"%s\"", snd_pcm_info_get_name(pcminfo));
                  inform("    Subdevice Name:    \"%s\"", snd_pcm_info_get_subdevice_name(pcminfo));
                }

                if (check_alsa_device(device_name, 1, 0, 0) > 0) {
                  inform("  This device seems suitable for use with Shairport Sync.");
                  if ((selems_if_has_db_playback(0, NULL, NULL)) ||
                      (selems_if_has_db_playback(1, NULL, NULL))) {

                    char fp[] = "  Possible mixers:     ";
                    char sp[] = "                       ";
                    int found =
                        selems_if_has_db_playback(0, fp,
                                                  sp); // omit mixers that also have a capture part
                    if (found > 0)
                      selems_if_has_db_playback(1, sp,
                                                sp); // include mixers that also have a capture part
                    else
                      selems_if_has_db_playback(1, fp,
                                                sp); // include mixers that also have a capture part
                  } else {
                    if (extended_output != 0)
                      inform("    No mixers usable by Shairport Sync.");
                  }
                  if (extended_output == 0) {
                    inform("  The following rate and format would be chosen by Shairport Sync in "
                           "\"auto\" "
                           "mode:");
                    inform("     Rate              Format");
                    check_alsa_device(device_name, 0, 1, 0);
                  } else {
                    inform("    Suitable rates and formats (suggested setting first):");
                    inform("     Rate              Formats");
                    check_alsa_device(device_name, 0, 0, 0);
                    inform("    Other rates and formats not compatible with Shairport Sync:");
                    inform("     Rate              Formats");
                    check_alsa_device(device_name, 0, 0, 1);
                  }
                } else if (check_alsa_device(device_name, 1, 0, 0) == -4) {
                  inform("  This device is already in use and can not be checked.");
                  inform("  To check it, take it out of use and try again.");
                } else if (check_alsa_device(device_name, 1, 0, 0) == -5) {
                  inform("  This device can not be accessed and so can not be checked.");
                  inform("  (Does it need to be configured or connected?)");
                } else if (check_alsa_device(device_name, 1, 0, 1) > 0) {
                  inform("  Shairport Sync can not use this device because it does not accept "
                         "suitable audio formats.");
                } else {
                  inform("  Shairport Sync can not use this device.");
                }

                /*
                // subdevices
                int count = snd_pcm_info_get_subdevices_count(pcminfo);
                inform("  Subdevices: %i/%i", snd_pcm_info_get_subdevices_avail(pcminfo), count);
                int idx;
                for (idx = 0; idx < (int)count; idx++) {
                        snd_pcm_info_set_subdevice(pcminfo, idx);
                        if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                                debug(1,"control digital audio playback info (%i): %s", card,
                snd_strerror(err)); } else { printf("  Subdevice #%i: %s\n", idx,
                snd_pcm_info_get_subdevice_name(pcminfo));
                        }
                }
                */
                inform(""); // newline
              }
              sub_device++;
            } while ((check_subdevices != 0) && (sub_device < sub_device_count));
        }
        snd_ctl_close(handle);
      next_device_name_on_card:
        free(device_on_card_name);
        device_on_card_hints++;
      }
      snd_device_name_free_hint(hints);
    }
    if (snd_card_next(&card_number) < 0) {
      debug(1, "snd_card_next");
      break;
    }
  }

  // now do a check on access to devices, even if they were found and listed.

  gid_t required_gid = 0;   // store the owner gid of the first inaccessible device
  int required_gid_set = 0; // flag to prevent overwrite of first device
  // get to /dev/snd
  char sound_dir[] = "/dev/snd";
  DIR *dp = opendir(sound_dir);
  if (dp != NULL) {
    int stat_error = 0;
    int dfd = dirfd(dp); /* Very, very unlikely to fail */
    struct dirent *dirp;
    int sound_devices_found = 0;
    int sound_devices_accessible = 0;
    while ((dirp = readdir(dp)) != NULL) {
      struct stat sb;
      if (fstatat(dfd, dirp->d_name, &sb, 0) == -1) {
        // debug(1, "fstatat(\"%s/%s\") failed (%d: %s)", sound_dir, dirp->d_name, errno,
        //       strerror(errno));
        stat_error = 1;
      } else {
        // debug(1, "fstatat(\"%s/%s\")", sound_dir, dirp->d_name);
        if (((sb.st_mode & S_IFMT) == S_IFCHR) || ((sb.st_mode & S_IFMT) == S_IFBLK)) {
          sound_devices_found++;
          char device_pathname[4096];
          snprintf(device_pathname, sizeof(device_pathname) - 1, "%s/%s", sound_dir, dirp->d_name);
          // debug(1,"Checking access to \"%s\"", device_pathname);
          if (access(device_pathname, R_OK | W_OK) == 0) {
            // debug(1,"Can access \"%s\".", dirp->d_name);
            sound_devices_accessible++;
          } else {
            // debug(1, "Unable to access \"%s\".", dirp->d_name);
            if (required_gid_set == 0) {
              required_gid = sb.st_gid;
              required_gid_set = 1;
            }
          }
        }
      }
    }
    if ((sound_devices_found == 0) && (stat_error == 0)) {
      inform("No sound devices were found."); // not necessarily an error
    } else {
      debug(1, "Devices found in the sound devices directory \"%s\": %d, devices accessible: %d.",
            sound_dir, sound_devices_found, sound_devices_accessible);
      if ((sound_devices_found != sound_devices_accessible) || (stat_error != 0)) {
        // get user name
        struct passwd *result;
        result = getpwuid(getuid());
        if (sound_devices_accessible == 0) {
          // get the name of the group of the first inaccessible device. In most cases, it will be
          // "audio" and will bethe same for all inaccessible devices.

          struct group *gr = getgrgid(required_gid);

          if (stat_error == 0) {
            inform(
                "This check can not be performed because the current user, \"%s\", does not have "
                "permission to access sound devices.",
                result->pw_name);
            inform(
                "Adding \"%s\" to the \"%s\" group may fix this. Alternatively, try running this "
                "tool as the \"root\" user.",
                result->pw_name, gr->gr_name);
          } else {
            inform(
                "This check can not be performed because the current user, \"%s\", does not have "
                "permission to examine the contents of the sound devices directory \"%s\".",
                result->pw_name, sound_dir);
            inform("Try running this tool as the \"root\" user.");
          }

          response = -1;
        } else {
          inform("This check can not be performed because the current user, \"%s\", does not have "
                 "permission to access all sound devices.",
                 result->pw_name);
          inform("To fix this, check the permissions of items in the standard sound device "
                 "directory \"%s\".",
                 sound_dir);
          inform("Alternatively, try running this tool as the \"root\" user.");
          response = -1;
        }
      }
    }
    closedir(dp);
  } else {
    response = -1;
    if (errno == ENOENT) {
      inform("The standard sound device directory \"%s\" was not found.", sound_dir);
    } else {
      inform("The standard sound device directory \"%s\" could not be accessed. (Error %d: %s).",
             sound_dir, errno, strerror(errno));
    }
  }
  return response;
}

int main(int argc, char *argv[]) {
  int debug_level = 0;
  int i;
  for (i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i] + 1, "V") == 0) {
#ifdef CONFIG_USE_GIT_VERSION_STRING
        if (git_version_string[0] != '\0')
          fprintf(stdout, "Version: %s\n", git_version_string);
        else
#endif
          fprintf(stdout, "Version: %s.\n", VERSION);
        exit(EXIT_SUCCESS);
      } else if (strcmp(argv[i] + 1, "vvv") == 0) {
        debug_level = 3;
      } else if (strcmp(argv[i] + 1, "vv") == 0) {
        debug_level = 2;
      } else if (strcmp(argv[i] + 1, "v") == 0) {
        debug_level = 1;
      } else if (strcmp(argv[i] + 1, "h") == 0) {
        fprintf(
            stdout,
            "This tool scans for ALSA devices that can be used by Shairport Sync.\n"
            "It does this by attempting to open each ALSA device for two-channel interleaved "
            "operation at\n"
            "frame rates that are multiples of 44100 with linear integer sample formats "
            "of 32, 24, 16 and 8 bits.\n"
            "If successful, it lists any decibel-mapped mixers found on the device for "
            "possible use by Shairport Sync.\n"
            "It also suggests the frame rate and format that would be chosen by Shairport Sync in "
            "automatic mode.\n"
            "Notes:\n"
            "1. This tool must be run by a user that is a member of the \"audio\" unix group\n"
            "   or by the root user. Otherwise no ALSA devices will be found.\n"
            "2. Make sure any HDMI devices you wish to check are plugged in, turned on\n"
            "   and enabled when the machine boots up. Reboot if necessary.\n"
            "3. If a device is in use, it can't be checked by this tool. In that case, you should\n"
            "   take the device out of use and run this tool again.\n"
            "4. If a device can not be accessed, it may mean that it needs to be configured or\n"
            "   connected to an active external device.\n"
            "5. Use the \"Device Full Name\" when specifying the device in the Shairport Sync\n"
            "   configuration file or on the Shairport Sync command line.\n"
            "   (The \"Short Name\" can change between reboots.)\n"

            "Command line arguments:\n"
            "    -e     extended information -- a little more information about each device,\n"
            "    -s     check every subdevice,\n"
            "    -V     print version,\n"
            "    -v     verbose log,\n"
            "    -vv    more verbose log,\n"
            "    -vvv   very verbose log,\n"
            "    -h     this help text.\n");
        exit(EXIT_SUCCESS);
      } else if (strcmp(argv[i] + 1, "e") == 0) {
        extended_output = 1;
      } else if (strcmp(argv[i] + 1, "s") == 0) {
        check_subdevices = 1;
      } else {
        fprintf(stdout, "%s -- unknown option. Program terminated.\n", argv[0]);
        exit(EXIT_FAILURE);
      }
    }
  }
  debug_init(debug_level, 0, 1, 1);
  debug(1, "startup.");

  return cards() ? 1 : 0;
}
