# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Provide an sdk location that is writable by the module
DISTCHECK_CONFIGURE_FLAGS = --with-sdkdir='$${includedir}/xorg'

SUBDIRS = src include
MAINTAINERCLEANFILES = ChangeLog INSTALL

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xorg-cmt.pc

.PHONY: ChangeLog INSTALL

# TODO(djkurtz): Resolve this build failure
#INSTALL:
#  $(INSTALL_CMD)

# TODO(djkurtz): Resolve this build failure
#ChangeLog:
# $(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

