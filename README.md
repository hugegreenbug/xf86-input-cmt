xf86-input-cmt
==============

X11 ChromiumOS touchpad driver ported to Linux

This driver depends on libgestures: https://github.com/hugegreenbug/libgestures and libevdevc: https://github.com/hugegreenbug/libevdevc

Compiling
==============
./apply_patches.sh
./configure --prefix=/usr
make
make install 

Configuring
=============
You will need to copy over the config files for the driver to the appropriate locations.  The config files are in the xorg-conf directory in this repo.  For a trackpad, copy over the 40-touchpad-cmt.conf, and a 50-touchpad-cmt file that matches your device to the xorg.conf.d directory for your system (/usr/share/X11/xorg.conf.d on Ubuntu). For all mice, copy 20-mouse.conf to /usr/share/X11/xorg.conf.d

If you use the Ubuntu package, the config files are installed in /usr/share/xf86-input-cmt.

You will also need to move your old config file out of the way or remove the previous driver.  If you were using the synaptics driver, move the synaptics.conf (/usr/share/X11/xorg.conf.d/50-synaptics.conf on Ubuntu) to something other than .conf or remove the synaptics driver.

Options can be modified with xinput.  You can list all the properties available to change:

xinput --list-props 12

Where 12 is the id of your trackpad. It may or may not be 12.  You can list all of your devices to find which one is your trackpad:

xinput

Place the options you wish to change in the touchpad config file that you copied over to /usr/share/X11/xorg.conf.d.

My tweaks for the Acer C720 are:
    Option          "Tap Timeout" "2.0"
    Option          "Two Finger Pressure Diff Thresh" "80.0"
    Option          "Tap Min Separation" "10.0"
    Option          "Tap Drag Enable" "1"
    Option          "Tap Drag Delay" "0.060000"
    Option          "Pointer Sensitivity" "4"
    Option          "Palm Pressure" "100"
    Option          "Stationary Wiggle Filter Enabled" "1"
    Option          "Smooth Accel" "1"
    Option          "Tap Minimum Pressure" "0.5"
    Option          "Finger Moving Hysteresis" "0.015"

Notes
============
Standard X acceleration settings will interfere with the driver settings and make the pointer less responsive. Disable them like this:

xinput set-ptr-feedback "trackpad_id" 0 0 1

Place that in your ~/.xsessionrc to make it permanant.

Packages
============
There is an ubuntu ppa with the latest releases here: https://launchpad.net/~hugegreenbug/+archive/cmt .

