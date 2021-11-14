## Building
### Prerequisites
```
# apt install build-essential git autoconf automake libtool libasound2-dev
```
### Download and Build
```
$ git clone https://github.com/mikebrady/alsaexplore.git
$ cd alsaexplore
$ autoreconf -fi
$ make
```
## Using
To run it from the directory in which it was compiled:
```
$ ./alsaexplore
```
