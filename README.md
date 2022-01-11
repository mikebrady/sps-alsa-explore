# sps-alsa-explore
## Purpose
This command-line application scans for ALSA devices that can be used by Shairport Sync.

It does this by attempting to open every ALSA device it can find for two-channel interleaved operation at
frame rates that are multiples of 44100 with linear integer sample formats of 32, 24, 16 and 8 bits.

If successful, it lists any decibel-mapped mixers found on the device for possible use by Shairport Sync.

It also suggests the frame rate and format that would be chosen by Shairport Sync in automatic mode.


## Building
### Prerequisites
```
# apt install --no-install-recommends build-essential git autoconf automake libtool libasound2-dev
```
### Download and Build
```
$ git clone https://github.com/mikebrady/sps-alsa-explore.git
$ cd sps-alsa-explore
$ autoreconf -fi
$ ./configure
$ make
```
## Using
To run it from the directory in which it was compiled:
```
$ ./sps-alsa-explore
```
