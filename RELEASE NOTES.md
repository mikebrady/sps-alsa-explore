Version: 1.2
===
This is version 1.2-dev.

Version: 1.2-dev
===
**Enhancement**
* If a device is identified as a HDMI device, use the `hdmi:` prefix rather than the `hw:` prefix.
* With the extra output option, `-e`, list speeds that Shairport Sync doesn't use.
* Suggest what to do if a HDMI device returns error 524 -- plug up target device, switch it on, reboot this device.
* Error 524 seems to mean the HDMI stuff hasn't/can't be initialised.

Version: 1.1-50-g62c540e
====
**Bug Fix**
* If a device could only support formats that were unsuitable for Shairport Sync, it was omitted from the list of devices completely. Fixed.

**Enhancement**
* Updated help messages.

Version: 1.1-34-gba97928
====
* Check for a variety of access or permission issues and try to output sensible messages. If necessary, discover the name of the unix group of one of the devices. Typically -- but not always -- all sound devices have the same group, and that group is `audio`.
* List the range of attenuation of any decibel-mapped mixers. It might be useful when trying to figure out which to use for Shairport Sync.

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
