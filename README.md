# Poor attemt at a Linux kernel driver to add support for Simucube wheelbases since 
# I can't get Linux and Steam games to behave with my peripherals e.g. wheelbase, wheel, pedals or joystick. 
# Proton borked pedals after 5.13, and as of writing still persists in 8.0-4.
# I will handle my specific peripherals separately Simfeel pedals / Polsimer Wheel and VKB joystick via
# HID-BPF instead, since these have incorrect descriptors according to Linux
# https://github.com/ValveSoftware/steam-runtime/issues/561
# https://github.com/ValveSoftware/Proton/issues/5126

## Known devices

* 16d0:0d5a Simucube 1
* 16d0:0d61 Simucube 2 Sport
* 16d0:0d60 Simucube 2 Pro
* 16d0:0d5f Simucube 2 Ultimate

## Installation

Compile and install the driver

```sh
make
sudo make install
```

Reload the new udev rules, depending on the Linux distribution, without rebooting:

```sh
sudo udevadm control --reload-rules && sudo udevadm trigger
```

This installs the kernel module `hid-simucube.ko` in the `hid` dir of the running kernel and puts `simucube.rules` into `/etc/udev/rules.d`. These rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.
The driver should get loaded automatically when the wheel is plugged.

### Packaging
# If I get a working driver I'll might try and make a dkms-package, but that is a big big if.

### General

Support for a bunch of effects, mostly copy-pasted from [new-lg4ff](https://github.com/berarma/new-lg4ff). 
Currently not supported effects: FF_FRICTION, FF_INERTIA

With Proton 7/8 the wheel is not detected properly when starting a game for the first time (ie, when a new proton-prefix is created). A possible workaround is to first start the game with Proton 6, and then switch to a later one.

### FFB in specific Games

Games that will be tested by me:

* AC / ACC
* BeamNG.drive
* rFactor2
* RRRE
  
### Device specific

Advanced functions of wheels/bases are available via sysfs. Base sysfs path:

`/sys/module/hid_simucube2/drivers/hid:simucube_2_sport/0003:16d0:0d61.*/`
# is 0003 static or unique depending on peripherals
# seems like it, since dmesg | grep "peripheral" always shows hid-generic 0003:xxxx:yyy. Same goes for "input:" entry where these contain ref. to 0003

#### Common

* set/get range: echo number in degrees to `range`
* get id of mounted wheel: `wheel_id`

#### Simucube

* tuning menu:
  * get/set tuning menu slot: echo number into `SLOT`
  * values get/set: `BLI DPR DRI FEI FF FOR SEN SHO SPR`
  * reset all tuning sets by echoing anything into `RESET`

#### Pedals

* loadcell adjustment: `load` (no readback yet)

# I might try and fork gotzl's tool project if I get as far as testing this driver.
To access advanced functions from user space please see the [hid-simucube-tools](https://github.com/gotzl/hid-fanatecff-tools) project which also aims to support LED/Display access from games.

## Disclaimer

I take absolutly _no_ responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though. ;)
