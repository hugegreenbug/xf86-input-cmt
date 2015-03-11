/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _PROPERTIES_H_
#define _PROPERTIES_H_

#include <gestures/gestures.h>

#include <xorg-server.h>
#include <xf86.h>
#include <xf86Xinput.h>


typedef struct {
    int area_left;
    int area_right;
    int area_top;
    int area_bottom;
    int res_y;
    int res_x;
    int orientation_minimum;
    int orientation_maximum;
    int raw_passthrough;
    GesturesPropBool dump_debug_log;
} CmtProperties, *CmtPropertiesPtr;

int PropertiesInit(DeviceIntPtr);
void PropertiesClose(DeviceIntPtr);

extern GesturesPropProvider prop_provider;

#endif
