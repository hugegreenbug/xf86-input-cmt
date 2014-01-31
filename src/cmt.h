/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _CMT_H_
#define _CMT_H_

#include <linux/input.h>

#include <gesture.h>
#include <properties.h>
// todo(denniskempin): allow libevdev to be included before X headers
#include <libevdev/libevdev.h>

#define DBG_VERB 7
#define DBG(info, format, ...) \
    xf86IDrvMsgVerb((info), X_INFO, DBG_VERB, "%s():%d: " format, \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define PROBE_DBG(info, format, ...) \
    xf86IDrvMsgVerb((info), X_PROBED, DBG_VERB, "%s():%d: " format, \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define CONFIG_DBG(info, format, ...) \
    xf86IDrvMsgVerb((info), X_CONFIG, DBG_VERB, "%s():%d: " format, \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ERR(info, ...) \
        xf86IDrvMsg((info), X_ERROR, ##__VA_ARGS__)

#define LONG_BITS (sizeof(long) * 8)

/* Number of longs needed to hold the given number of bits */
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

/* Axes numbers. */
enum CMT_AXIS {
    CMT_AXIS_X = 0,
    CMT_AXIS_Y,
    CMT_AXIS_ORDINAL_X,
    CMT_AXIS_ORDINAL_Y,
    CMT_AXIS_SCROLL_X,
    CMT_AXIS_SCROLL_Y,
    CMT_AXIS_FLING_STATE,
    CMT_AXIS_DBL_FLING_VX,
    CMT_AXIS_DBL_FLING_VY,
    CMT_AXIS_METRICS_TYPE,
    CMT_AXIS_METRICS_DATA1,
    CMT_AXIS_METRICS_DATA2,
    CMT_AXIS_DBL_START_TIME,
    CMT_AXIS_DBL_END_TIME,
    CMT_AXIS_FINGER_COUNT,
    CMT_AXIS_MT_POSITION_X,
    CMT_AXIS_MT_POSITION_Y,
    CMT_AXIS_MT_PRESSURE,
    CMT_AXIS_MT_TOUCH_MAJOR,
    CMT_AXIS_TOUCH_TIMESTAMP
};

#define CMT_NUM_AXES (CMT_AXIS_TOUCH_TIMESTAMP - CMT_AXIS_X + 1)
#define CMT_NUM_MT_AXES (CMT_AXIS_TOUCH_TIMESTAMP - CMT_AXIS_MT_POSITION_X + 1)

/* Button numbers. */
enum CMT_BUTTON {
    CMT_BTN_LEFT = 1,
    CMT_BTN_MIDDLE,
    CMT_BTN_RIGHT,
    CMT_BTN_BACK,
    CMT_BTN_FORWARD
};

#define CMT_NUM_BUTTONS (CMT_BTN_FORWARD - CMT_BTN_LEFT + 1)

typedef struct {
    CmtProperties props;
    EventStateRec evstate;
    GestureRec gesture;
    GesturesProp* prop_list;
    Evdev evdev;

    char* device;
    long  handlers;
} CmtDeviceRec, *CmtDevicePtr;

#endif
