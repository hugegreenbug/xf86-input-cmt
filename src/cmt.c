/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cmt.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <exevents.h>
#include <X11/extensions/XI.h>
#include <X11/Xatom.h>
#include <xf86Xinput.h>
#include <xf86_OSproc.h>
#include <xkbsrv.h>
#include <xserver-properties.h>

#include "properties.h"

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 12
#error Unsupported XInput version. Major version 12 and above required.
#endif

#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X   "Abs Dbl Ordinal X"
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y   "Abs Dbl Ordinal Y"

#define AXIS_LABEL_PROP_ABS_FLING_STATE    "Abs Fling State"
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VX   "Abs Dbl Fling X Velocity"
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VY   "Abs Dbl Fling Y Velocity"

#define AXIS_LABEL_PROP_ABS_METRICS_TYPE      "Abs Metrics Type"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1 "Abs Dbl Metrics Data 1"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2 "Abs Dbl Metrics Data 2"

#define AXIS_LABEL_PROP_ABS_DBL_START_TIME "Abs Dbl Start Timestamp"
#define AXIS_LABEL_PROP_ABS_DBL_END_TIME   "Abs Dbl End Timestamp"

#define AXIS_LABEL_PROP_ABS_FINGER_COUNT   "Abs Finger Count"

#define AXIS_LABEL_PROP_ABS_TOUCH_TIMESTAMP "Touch Timestamp"

/**
 * Forward declarations
 */
static int PreInit(InputDriverPtr, InputInfoPtr, int);
static void UnInit(InputDriverPtr, InputInfoPtr, int);

static pointer Plug(pointer, pointer, int*, int*);
static void Unplug(pointer);

static int DeviceControl(DeviceIntPtr, int);
static void ReadInput(InputInfoPtr);

static Bool DeviceInit(DeviceIntPtr);
static Bool DeviceOn(DeviceIntPtr);
static Bool DeviceOff(DeviceIntPtr);
static Bool DeviceClose(DeviceIntPtr);

static Bool OpenDevice(InputInfoPtr);
static int InitializeXDevice(DeviceIntPtr dev);

static void libevdev_log_x(void* udata, int level, const char* format, ...)
    _X_ATTRIBUTE_PRINTF(3, 4);

/*
 * Implementation of log method for libevdev
 */
static void libevdev_log_x(void* udata, int level, const char* format, ...) {
  InputInfoPtr info = udata;
  int verb;
  int type;

  va_list args;
  va_start(args, format);
  if (level == LOGLEVEL_DEBUG) {
    type = X_INFO;
    verb = DBG_VERB;
  } else if (level == LOGLEVEL_WARNING) {
    type = X_WARNING;
    verb = -1;
  } else {
    type = X_ERROR;
    verb = -1;
  }
  xf86VIDrvMsgVerb(info, type, verb, format, args);
  va_end(args);
}

/**
 * X Input driver information and PreInit / UnInit routines
 */
_X_EXPORT InputDriverRec CMT = {
    1,
    (char*)"cmt",
    NULL,
    PreInit,
    UnInit,
    NULL,
    0
};

static int
PreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    CmtDevicePtr cmt;
    int rc;

    DBG(info, "NewPreInit\n");

    cmt = calloc(1, sizeof(*cmt));
    if (!cmt)
        return BadAlloc;

    info->device_control          = DeviceControl;
    info->read_input              = ReadInput;
    info->control_proc            = NULL;
    info->switch_mode             = NULL;  /* Only support Absolute mode */
    info->private                 = cmt;
    info->fd                      = -1;

    cmt->evdev.log = &libevdev_log_x;
    cmt->evdev.log_udata = info;
    cmt->evdev.fd = info->fd;
    cmt->evdev.evstate = &cmt->evstate;
    cmt->evdev.syn_report = &Gesture_Process_Slots;
    cmt->evdev.syn_report_udata = &cmt->gesture;

    rc = OpenDevice(info);
    if (rc != Success)
        goto Error_OpenDevice;

    rc = Event_Init(&cmt->evdev);
    if (rc != Success) {
        if (rc == ENOMEM) {
            rc = BadAlloc;
        }
        goto Error_Event_Init;
    }

    // The cmt driver currently powers mice, multi-touch mice and touchpads.
    // We list mice as XI_MOUSE and the others as XI_TOUCHPAD.
    if (cmt->evdev.info.evdev_class == EvdevClassMouse)
      info->type_name = (char*)XI_MOUSE;
    else
      info->type_name = (char*)XI_TOUCHPAD;

    xf86ProcessCommonOptions(info, info->options);

    if (info->fd >= 0)
        info->fd = EvdevClose(&cmt->evdev);

    rc = Gesture_Init(&cmt->gesture, Event_Get_Slot_Count(&cmt->evdev));
    if (rc != Success)
        goto Error_Gesture_Init;

    return Success;

Error_Gesture_Init:
    Event_Free(&cmt->evdev);
Error_Event_Init:
    if (info->fd >= 0)
      info->fd = EvdevClose(&cmt->evdev);
Error_OpenDevice:
    free(cmt);
    info->private = NULL;
    return rc;
}

static void
UnInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    CmtDevicePtr cmt;

    if (!info)
        return;

    cmt = info->private;
    DBG(info, "UnInit\n");

    if (cmt) {
        Gesture_Free(&cmt->gesture);
        free(cmt->device);
        cmt->device = NULL;
        Event_Free(&cmt->evdev);
        free(cmt);
        info->private = NULL;
    }
    xf86DeleteInput(info, flags);
}

/**
 * X input driver entry points
 */
static Bool
DeviceControl(DeviceIntPtr dev, int mode)
{
    switch (mode) {
    case DEVICE_INIT:
        return DeviceInit(dev);

    case DEVICE_ON:
        return DeviceOn(dev);

    case DEVICE_OFF:
        return DeviceOff(dev);

    case DEVICE_CLOSE:
        return DeviceClose(dev);
    }

    return BadValue;
}

static void
ReadInput(InputInfoPtr info)
{
    CmtDevicePtr cmt = info->private;

    int err = EvdevRead(&cmt->evdev);
    if (err != Success) {
      if (err == ENODEV) {
          xf86RemoveEnabledDevice(info);
          info->fd = EvdevClose(&cmt->evdev);
      } else if (err != EAGAIN) {
          ERR(info, "Read error: %s\n", strerror(err));
      }
    }
}

/**
 * device control event handlers
 */
static Bool
DeviceInit(DeviceIntPtr dev)
{
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;
    int rc;

    DBG(info, "DeviceInit\n");

    InitializeXDevice(dev);
    dev->public.on = FALSE;

    rc = PropertiesInit(dev);
    if (rc != Success)
        return rc;

    Gesture_Device_Init(&cmt->gesture, dev);

    return Success;
}

static Bool
DeviceOn(DeviceIntPtr dev)
{
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;
    int rc;

    DBG(info, "DeviceOn\n");

    rc = OpenDevice(info);
    if (rc != Success)
        return rc;
    Event_Open(&cmt->evdev);

    xf86AddEnabledDevice(info);
    dev->public.on = TRUE;
    Gesture_Device_On(&cmt->gesture);
    return Success;
}

static Bool
DeviceOff(DeviceIntPtr dev)
{
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;

    DBG(info, "DeviceOff\n");

    dev->public.on = FALSE;
    Gesture_Device_Off(&cmt->gesture);
    if (info->fd != -1) {
        xf86RemoveEnabledDevice(info);
        info->fd = EvdevClose(&cmt->evdev);
    }
    return Success;
}

static Bool
DeviceClose(DeviceIntPtr dev)
{
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;

    DBG(info, "DeviceClose\n");

    DeviceOff(dev);
    Gesture_Device_Close(&cmt->gesture);
    PropertiesClose(dev);
    return Success;
}

/**
 * Open Device Node
 */
static Bool
OpenDevice(InputInfoPtr info)
{
    CmtDevicePtr cmt = info->private;

    if (!cmt->device) {
        cmt->device = xf86CheckStrOption(info->options, "Device", NULL);
        if (!cmt->device) {
            ERR(info, "No Device specified.\n");
            return BadValue;
        }
        xf86IDrvMsg(info, X_CONFIG, "Opening Device: \"%s\"\n", cmt->device);
    }

    if (info->fd < 0) {
        do {
            info->fd = EvdevOpen(&cmt->evdev, cmt->device);
        } while (info->fd < 0 && errno == EINTR);

        if (info->fd < 0) {
            ERR(info, "Cannot open \"%s\".\n", cmt->device);
            return BadValue;
        }
    }

    return Success;
}


/**
 * Setup X Input Device Classes
 */

/*
 *  Alter the control parameters for the mouse. Note that all special
 *  protocol values are handled by dix.
 */
static void
PointerCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
}

static void
KeyboardCtrl(DeviceIntPtr device, KeybdCtrl *ctrl)
{
}

static Atom
InitAtom(const char* name)
{
    Atom atom = XIGetKnownProperty(name);
    if (!atom)
        atom = MakeAtom(name, strlen(name), TRUE);
    return atom;
}

static int
InitializeXDevice(DeviceIntPtr dev)
{
    static const char* axes_names[CMT_NUM_AXES] = {
        AXIS_LABEL_PROP_REL_X,
        AXIS_LABEL_PROP_REL_Y,
        AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X,
        AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y,
        AXIS_LABEL_PROP_REL_HWHEEL,
        AXIS_LABEL_PROP_REL_WHEEL,
        AXIS_LABEL_PROP_ABS_FLING_STATE,
        AXIS_LABEL_PROP_ABS_DBL_FLING_VX,
        AXIS_LABEL_PROP_ABS_DBL_FLING_VY,
        AXIS_LABEL_PROP_ABS_METRICS_TYPE,
        AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1,
        AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2,
        AXIS_LABEL_PROP_ABS_DBL_START_TIME,
        AXIS_LABEL_PROP_ABS_DBL_END_TIME,
        AXIS_LABEL_PROP_ABS_FINGER_COUNT,
        AXIS_LABEL_PROP_ABS_MT_POSITION_X,
        AXIS_LABEL_PROP_ABS_MT_POSITION_Y,
        AXIS_LABEL_PROP_ABS_MT_PRESSURE,
        AXIS_LABEL_PROP_ABS_MT_TOUCH_MAJOR,
        AXIS_LABEL_PROP_ABS_TOUCH_TIMESTAMP,
    };
    static const char* btn_names[CMT_NUM_BUTTONS] = {
        BTN_LABEL_PROP_BTN_LEFT,
        BTN_LABEL_PROP_BTN_MIDDLE,
        BTN_LABEL_PROP_BTN_RIGHT,
        BTN_LABEL_PROP_BTN_BACK,
        BTN_LABEL_PROP_BTN_FORWARD,
    };
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;

    XkbRMLVOSet rmlvo = { 0 };
    Atom axes_labels[CMT_NUM_AXES] = { 0 };
    Atom btn_labels[CMT_NUM_BUTTONS] = { 0 };
    /* Map our button numbers to standard ones. */
    CARD8 map[CMT_NUM_BUTTONS + 1] = {
        0,  /* Ignored */
        1,
        2,
        3,
        8,  /* Back */
        9   /* Forward */
    };
    int i;

    /* TODO: Prop to adjust button mapping */
    for (i = 0; i < CMT_NUM_BUTTONS; i++)
        btn_labels[i] = XIGetKnownProperty(btn_names[i]);

    for (i = 0; i < CMT_NUM_AXES; i++)
        axes_labels[i] = InitAtom(axes_names[i]);

    /* initialize mouse emulation valuators */
    InitPointerDeviceStruct((DevicePtr)dev,
                            map,
                            CMT_NUM_BUTTONS, btn_labels,
                            PointerCtrl,
                            GetMotionHistorySize(),
                            CMT_NUM_AXES, axes_labels);

    for (i = 0; i < CMT_NUM_AXES; i++) {
        int mode = (i == CMT_AXIS_X || i == CMT_AXIS_Y) ? Relative : Absolute;
        if (i >= CMT_AXIS_MT_POSITION_X)
            break;
        xf86InitValuatorAxisStruct(
            dev, i, axes_labels[i], -1, -1, 1, 0, 1, mode);
        xf86InitValuatorDefaults(dev, i);
    }

    /* initialize raw touch valuators */
    InitTouchClassDeviceStruct(dev, Event_Get_Slot_Count(&cmt->evdev),
                               XIDependentTouch, CMT_NUM_MT_AXES);

    for (i = 0; i < CMT_NUM_AXES; i++) {
        int mode = (i == CMT_AXIS_X || i == CMT_AXIS_Y) ? Relative : Absolute;
        int input_axis = 0;
        if (i == CMT_AXIS_TOUCH_TIMESTAMP) {
            xf86InitValuatorAxisStruct(dev, i, axes_labels[i],
                0, INT_MAX, 1, 0, 1, Absolute);
            continue;
        }

        if (i == CMT_AXIS_MT_POSITION_X)
            input_axis = ABS_MT_POSITION_X;
        else if (i == CMT_AXIS_MT_POSITION_Y)
            input_axis = ABS_MT_POSITION_Y;
        else if (i == CMT_AXIS_MT_PRESSURE)
            input_axis = ABS_MT_PRESSURE;
        else if (i == CMT_AXIS_MT_TOUCH_MAJOR)
            input_axis = ABS_MT_TOUCH_MAJOR;
        else
            continue;
        xf86InitValuatorAxisStruct(
                dev, i, axes_labels[i],
                cmt->evdev.info.absinfo[input_axis].minimum,
                cmt->evdev.info.absinfo[input_axis].maximum,
                cmt->evdev.info.absinfo[input_axis].resolution,
                0,
                cmt->evdev.info.absinfo[input_axis].resolution,
                mode);
        xf86InitValuatorDefaults(dev, i);
    }

    /* Initialize keyboard device struct. Based on xf86-input-evdev,
       do not allow any rule/layout/etc changes. */
    xf86ReplaceStrOption(info->options, "xkb_rules", "evdev");
    rmlvo.rules = xf86SetStrOption(info->options, "xkb_rules", NULL);
    rmlvo.model = xf86SetStrOption(info->options, "xkb_model", NULL);
    rmlvo.layout = xf86SetStrOption(info->options, "xkb_layout", NULL);
    rmlvo.variant = xf86SetStrOption(info->options, "xkb_variant", NULL);
    rmlvo.options = xf86SetStrOption(info->options, "xkb_options", NULL);

    InitKeyboardDeviceStruct(dev, &rmlvo, NULL, KeyboardCtrl);
    XkbFreeRMLVOSet(&rmlvo, FALSE);

    return Success;
}


/**
 * X module information and plug / unplug routines
 */
static XF86ModuleVersionInfo versionRec =
{
    "cmt",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData cmtModuleData =
{
    &versionRec,
    Plug,
    Unplug
};


static pointer
Plug(pointer module, pointer options, int* errmaj, int* errmin)
{
    xf86AddInputDriver(&CMT, module, 0);
    return module;
}

static void
Unplug(pointer p)
{
}
