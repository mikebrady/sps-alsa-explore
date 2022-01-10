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
