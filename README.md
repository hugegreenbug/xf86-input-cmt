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


Packages
============
There is an ubuntu ppa with the latest releases here: https://launchpad.net/~hugegreenbug/+archive/cmt .
