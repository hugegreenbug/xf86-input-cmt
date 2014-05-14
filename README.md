xf86-input-cmt
==============

X11 ChromiumOS touchpad driver ported to Linux

This driver depends on libgestures: https://github.com/hugegreenbug/libgestures and libevdevc: https://github.com/hugegreenbug/libevdevc

Compiling
==============
./configure --prefix=/usr
make
make install

Configuring
=============
You will need to copy over the config files for the driver to the appropriate locations.  The config files are in the xorg-conf directory in this repo.  For a trackpad, copy over the 20-mouse.conf, 40-touchpad-cmt.conf, and a 50-touchpad-cmt file that matches your device to the xorg.conf.d directory for your system (/usr/share/X11/xorg.conf.d on Ubuntu Saucy).

If you use the Ubuntu package, the config files are installed in /usr/share/xf86-input-cmt.

You will also need to move your old config file out of the way or remove the previous driver.  If you were using the synaptics driver, move the synaptics.conf (/usr/share/X11/xorg.conf.d/50-synaptics.conf on Ubuntu) to something other than .conf or remove the synaptics driver.

Options can be modified with xinput.  Place your xinput commands in ~/.xsessionrc to have them run when your X11 session starts. Here is my .xsessionrc that enables tag and drag, modifies the sensitivity of tap and drag, and increases the speed of the pointer:

xinput set-int-prop 12 "Tap Drag Enable" 8 1
xinput set-float-prop 12 "Tap Drag Delay" 0.060000
xinput set-int-prop 12 "Pointer Sensitivity" 32 4

Packages
============
There is an ubuntu ppa with the latest releases here: https://launchpad.net/~hugegreenbug/+archive/cmt .
