Version: 1.1-19-gf470a24
====
**Enhancement**
* If no soundcards can be found, suggest that the user should be in the `audio` unix group or that the program should be run as `root`.
Also update the help text. Thanks to [th0u](https://github.com/th0u) for [#1](https://github.com/mikebrady/sps-alsa-explore/issues/1)!

Version: 1.1-17-g4af18f7
====
**Enhancements**
* Issue a sensible message when a device exists but can't be accessed. It's not clear what this means, but perhaps it means that it needs configuration or perhaps it needs to have external equipment connected and active.
* Add the option `-s` to explore every subdevice where there is more than one on a device.
* Present the simplest device name according to the following rules for subdevices and devices:

  1. If no subdevice is specified, omit it -- a subdevice will be automatically chosen.
  2. If no subdevice is specified and the device number is zero, omit the device number as well -- the default device number is 0.

   See the "Concepts" section [here](https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture) for more information about naming.

Version: 1.1-8-g620f449
====
**Enhancement**
* Issue a sensible message when a device can't be opened because it's already in use.

**Bug Fix**
* Check that the frame rate set is equal to the frame rate requested when checking if a particular frame rate is supported.

Version: 1.1-1-g864d9cd
====
Initial version. Not extensively tested. Error messages may be misleading.
