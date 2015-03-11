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
You will need to copy over the config files for the driver to the appropriate locations.  The config files are in the xorg-conf directory in this repo.  For a trackpad, copy over the 20-mouse.conf, 40-touchpad-cmt.conf, and a 50-touchpad-cmt file that matches your device to the xorg.conf.d directory for your system (/usr/share/X11/xorg.conf.d on Ubuntu Saucy).

If you use the Ubuntu package, the config files are installed in /usr/share/xf86-input-cmt.

You will also need to move your old config file out of the way or remove the previous driver.  If you were using the synaptics driver, move the synaptics.conf (/usr/share/X11/xorg.conf.d/50-synaptics.conf on Ubuntu) to something other than .conf or remove the synaptics driver.

Options can be modified with xinput.  You can list all the properties available to change:

xinput --list-props 12

Where 12 is the id of your trackpad. It may or may not be 12.  You can list all of your devices to find which one is your trackpad:

xinput

Place your xinput commands in ~/.xsessionrc to have them run when your X11 session starts. Here is my .xsessionrc that enables tag and drag, modifies the sensitivity of tap and drag, increases the speed of the pointer, and increases the sensitivity of two finger scrolling:

ID=`xinput | grep cyapa | cut -f 2 | sed -e 's/id=//'`

xinput --set-int-prop $ID "Tap Drag Enable" 8 1

xinput --set-float-prop $ID "Tap Drag Delay" 0.060000

xinput --set-int-prop $ID "Pointer Sensitivity" 32 4


For Australian scrolling:

xinput --set-int-prop $ID "Scroll Australian" 8 1


To speed up the pointer:

xinput --set-float-prop $ID "Point Y Out Scale" 1.3

xinput --set-float-prop $ID "Point X Out Scale" 1.3


For faster acceleration:

xinput --set-float-prop $ID "Quick Acceleration Factor" 2

To disable tapping:

xinput --set-int-prop $ID "Tap Enable" 8 0


To disable right clicking on the right side of the touchpad's hardware button:

xinput --set-int-prop $ID "Button Right Click Enable" 8 0


Packages
============
There is an ubuntu ppa with the latest releases here: https://launchpad.net/~hugegreenbug/+archive/cmt .

