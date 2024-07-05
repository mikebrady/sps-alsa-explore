# sps-alsa-explore
## Purpose
This command-line application scans for `alsa` devices that can be used by Shairport Sync.

It does this by attempting to open every `alsa` device it can find for two-channel interleaved operation at
frame rates that are multiples of 44100 with linear integer sample formats of 32, 24, 16 and 8 bits.

If successful, it lists any decibel-mapped mixers found on the device for possible use by Shairport Sync.

It also suggests the frame rate and format that would be chosen by Shairport Sync in automatic mode.

## Docker Image
`sps-alsa-explore` is available on the Docker Hub at [mikebrady/sps-alsa-explore](https://hub.docker.com/r/mikebrady/sps-alsa-explore).
Alternatively, you can build it using the following guide.

## Building
In the commands below, note the convention that a `#` prompt means you are in superuser mode and a `$` prompt means you are in a regular unprivileged user mode. You can use `sudo` *("SUperuser DO")* to temporarily promote yourself from user to superuser, if permitted. For example, if you want to execute `apt-get update` in superuser mode and you are in user mode, enter `sudo apt-get update`.
### Prerequisites
```
# apt update
# apt install --no-install-recommends build-essential git autoconf automake libtool libasound2-dev
```
### Download, Build, Install
```
$ git clone https://github.com/mikebrady/sps-alsa-explore.git
$ cd sps-alsa-explore
$ autoreconf -fi
$ ./configure
$ make
# make install
```
## Use and Sample Output
Note that the user running this tool must be a member of the `audio` group, or must be the `root` user.
To run the tool from the directory in which it was compiled:
```
$ ./sps-alsa-explore
> Device:              "hw:Intel"
  Short Name:          "hw:0"
  This device seems suitable for use with Shairport Sync.
  Possible mixers:     "Master"              Range:  74.00 dB
                       "PCM"                 Range:  51.00 dB
  The following rate and format will be chosen by Shairport Sync in "auto" mode:
     Rate              Format
     44100             S16_LE

> Device:              "hw:sndrpihifiberry"
  Short Name:          "hw:1"
  This device is already in use and can not be checked.
  To check it, take it out of use and try again.

> Device:              "hw:vc4hdmi"
  Short Name:          "hw:2"
  This device can not be accessed and so can not be checked.
  (Does it need to be configured or connected?)
```
Unfortunately, there doesn't seem to be a consistent or logical way to tell which mixer is the best one to use.
