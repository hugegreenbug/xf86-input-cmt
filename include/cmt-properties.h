/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _CMT_PROPERTIES_H_
#define _CMT_PROPERTIES_H_

/**
 * Descriptions of properties exported by the driver.
 *
 * XI_PROP_*  - X Input Properties common to all X input drivers
 * CMT_PROP_* - Device Property name, used in .conf files and with xinput
 */

#ifndef XI_PROP_PRODUCT_ID
#define XI_PROP_PRODUCT_ID "Device Product ID"
#endif

#ifndef XI_PROP_VENDOR_ID
#define XI_PROP_VENDOR_ID "Device Vendor ID"
#endif

#ifndef XI_PROP_DEVICE_NODE
#define XI_PROP_DEVICE_NODE "Device Node"
#endif

/* 32 bit */
#define CMT_PROP_AREA_LEFT   "Active Area Left"
#define CMT_PROP_AREA_RIGHT  "Active Area Right"
#define CMT_PROP_AREA_TOP    "Active Area Top"
#define CMT_PROP_AREA_BOTTOM "Active Area Bottom"

/* 32 bit */
#define CMT_PROP_RES_Y       "Vertical Resolution"
#define CMT_PROP_RES_X       "Horizontal Resolution"

/* 32 bit */
#define CMT_PROP_ORIENTATION_MINIMUM "Orientation Minimum"
#define CMT_PROP_ORIENTATION_MAXIMUM "Orientation Maximum"

/* Bool */
#define CMT_PROP_SCROLL_BTN  "Scroll Buttons"
#define CMT_PROP_SCROLL_AXES "Scroll Axes"
#define CMT_PROP_DUMP_DEBUG_LOG "Dump Debug Log"
#define CMT_PROP_RAW_TOUCH_PASSTHROUGH "Raw Touch Passthrough"

#endif
