/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gesture.h"

#include <time.h>

#include <gestures/gestures.h>
#include <xorg/xf86_OSproc.h>

#include "cmt.h"
#include "properties.h"

// Helper for bit operations
#define LONG_BITS (sizeof(long) * 8)

// Implementation of inline bit operations
static inline bool TestBit(int bit, unsigned long* array)
{
    return !!(array[bit / LONG_BITS] & (1L << (bit % LONG_BITS)));
}

// This array maps input_event button types to gestures buttons
#define EVDEV_BUTTON_MAP_SIZE 7
static const int kEvdevButtonMap[EVDEV_BUTTON_MAP_SIZE][2] = {
    {BTN_LEFT, GESTURES_BUTTON_LEFT},
    {BTN_MIDDLE, GESTURES_BUTTON_MIDDLE},
    {BTN_RIGHT, GESTURES_BUTTON_RIGHT},
    {BTN_BACK, GESTURES_BUTTON_BACK},
    {BTN_SIDE, GESTURES_BUTTON_BACK},
    {BTN_FORWARD, GESTURES_BUTTON_FORWARD},
    {BTN_EXTRA, GESTURES_BUTTON_FORWARD}
};

// Conversion from kernel key codes to xorg key codes
#define MIN_KEYCODE 8

/*
 * Gestures timer functions
 */
static GesturesTimer* Gesture_TimerCreate(void*);
static void Gesture_TimerSet(void*,
                             GesturesTimer*,
                             stime_t,
                             GesturesTimerCallback,
                             void*);
static void Gesture_TimerCancel(void*, GesturesTimer*);
static void Gesture_TimerFree(void*, GesturesTimer*);

static CARD32 Gesture_TimerCallback(OsTimerPtr, CARD32, pointer);

struct GesturesTimer {
    OsTimerPtr timer;
    GesturesTimerCallback callback;
    void* callback_data;
    int is_monotonic:1;
};

static GesturesTimerProvider Gesture_GesturesTimerProvider = {
    .create_fn = Gesture_TimerCreate,
    .set_fn = Gesture_TimerSet,
    .cancel_fn = Gesture_TimerCancel,
    .free_fn = Gesture_TimerFree
};

/*
 * Callback for Gestures library.
 */
static void Gesture_Gesture_Ready(void* client_data,
                                  const struct Gesture* gesture);

static enum GestureInterpreterDeviceClass Gesture_Device_Class(EvdevClass cls);

int
Gesture_Init(GesturePtr rec, size_t max_fingers)
{
    rec->interpreter = NewGestureInterpreter();
    rec->slot_states = NULL;

    if (!rec->interpreter)
        return !Success;
    rec->fingers = malloc(max_fingers * sizeof(struct FingerState));
    if (!rec->fingers)
        goto Error_Alloc_Fingers;
    rec->mask = valuator_mask_new(MAX_VALUATORS);
    if (!rec->mask)
        goto Error_Alloc_Mask;
    return Success;

Error_Alloc_Mask:
    free(rec->fingers);
    rec->fingers = NULL;
Error_Alloc_Fingers:
    DeleteGestureInterpreter(rec->interpreter);
    rec->interpreter = NULL;
    return BadAlloc;
}

void
Gesture_Free(GesturePtr rec)
{
    // free gesture interpreter first, this will cancel all timers.
    DeleteGestureInterpreter(rec->interpreter);
    rec->interpreter = NULL;
    rec->dev = NULL;

    valuator_mask_free(&rec->mask);

    if (rec->fingers) {
        free(rec->fingers);
        rec->fingers = NULL;
    }
    if(rec->slot_states) {
        free(rec->slot_states);
        rec->slot_states = NULL;
    }
}

void
Gesture_Device_Init(GesturePtr rec, DeviceIntPtr dev)
{
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;
    CmtPropertiesPtr props = &cmt->props;
    EventStatePtr evstate = &cmt->evstate;
    struct HardwareProperties hwprops;
    EvdevPtr evdev = &cmt->evdev;
    int i;

    /* Store the device for which to generate gestures */
    rec->dev = dev;

    /* TODO: support different models */
    hwprops.left            = props->area_left;
    hwprops.top             = props->area_top;
    hwprops.right           = props->area_right;
    hwprops.bottom          = props->area_bottom;
    hwprops.res_x           = props->res_x;
    hwprops.res_y           = props->res_y;
    hwprops.screen_x_dpi    = 133;
    hwprops.screen_y_dpi    = 133;
    hwprops.orientation_minimum = props->orientation_minimum;
    hwprops.orientation_maximum = props->orientation_maximum;
    hwprops.max_finger_cnt  = evstate->slot_count;
    hwprops.max_touch_cnt   = Event_Get_Touch_Count_Max(evdev);
    hwprops.supports_t5r2   = Event_Get_T5R2(evdev);
    hwprops.support_semi_mt = Event_Get_Semi_MT(evdev);
    /* buttonpad means a physical button under the touch surface */
    hwprops.is_button_pad   = Event_Get_Button_Pad(evdev);
    hwprops.has_wheel       = TestBit(REL_WHEEL, evdev->info.rel_bitmask) ||
                              TestBit(REL_HWHEEL, evdev->info.rel_bitmask);

    GestureInterpreterSetPropProvider(rec->interpreter, &prop_provider,
                                      rec->dev);
    GestureInterpreterInitialize(
            rec->interpreter,
            Gesture_Device_Class(cmt->evdev.info.evdev_class));
    GestureInterpreterSetHardwareProperties(rec->interpreter, &hwprops);

    rec->slot_states = malloc(sizeof(int) * evstate->slot_count);
    if (!rec->slot_states) {
        ERR(info, "BadAlloc: rec->slot_states");
        return;
    }
    for (i = 0; i < evstate->slot_count; ++i)
        rec->slot_states[i] = SLOT_STATUS_FREE;
}

void
Gesture_Device_On(GesturePtr rec)
{
    GestureInterpreterSetTimerProvider(rec->interpreter,
                                       &Gesture_GesturesTimerProvider,
                                       rec->dev);
    GestureInterpreterSetCallback(rec->interpreter, &Gesture_Gesture_Ready,
                                  rec);
}

void
Gesture_Device_Off(GesturePtr rec)
{
    GestureInterpreterSetCallback(rec->interpreter, NULL, NULL);
}

void
Gesture_Device_Close(GesturePtr rec)
{
    GestureInterpreterSetPropProvider(rec->interpreter, NULL, NULL);
    GestureInterpreterSetTimerProvider(rec->interpreter, NULL, NULL);
}

void
Gesture_Process_Slots(void* vrec,
                      EventStatePtr evstate,
                      struct timeval* tv)
{
    GesturePtr rec = vrec;
    DeviceIntPtr dev = rec->dev;
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;
    EvdevPtr evdev = &cmt->evdev;
    ValuatorMask* mask = rec->mask;
    int i;
    MtSlotPtr slot;
    struct HardwareState hwstate = { 0 };
    int current_finger;
    bool has_gesture_fingers = false;
    unsigned long key_state_diff[NLONGS(KEY_CNT)];
    int code;
    int value;

    if (!rec->interpreter || ! rec->slot_states)
        return;

    /* handle changed keys */
    for (i = 0; i < NLONGS(KEY_CNT); ++i) {
        key_state_diff[i] = evdev->key_state_bitmask[i] ^
                            cmt->prev_key_state[i];
    }
    for (i = 0; i < KEY_CNT; ++i) {
        if (TestBit(i, key_state_diff)) {
            code = i + MIN_KEYCODE;
            value = TestBit(i, evdev->key_state_bitmask);
            xf86PostKeyboardEvent(dev, code, value);
        }
    }
    for (i = 0; i < NLONGS(KEY_CNT); ++i) {
        cmt->prev_key_state[i] = evdev->key_state_bitmask[i];
    }
    /* zero initialize all FingerStates to clear out previous state. */
    memset(rec->fingers, 0,
           Event_Get_Slot_Count(evdev) * sizeof(struct FingerState));

    /* clear out previous state from valuator */
    valuator_mask_zero(mask);

    if (cmt->props.raw_passthrough) {
        for (i = 0; i < evstate->slot_count; i++) {
            slot = &evstate->slots[i];

            /* send TouchEnd for lifted fingers */
            if (slot->tracking_id == -1) {
                if (rec->slot_states[i] == SLOT_STATUS_RAW) {
                    xf86PostTouchEvent(dev, i, XI_TouchEnd, 0, mask);
                }
                rec->slot_states[i] = SLOT_STATUS_FREE;
                continue;
            }

            /*
             * valuators 0 (CMT_AXIS_X) and 1 (CMT_AXIS_Y) are hardcoded into
             * X.org as finger position, so we need to set those too.
             */
            valuator_mask_set_double(mask, CMT_AXIS_MT_POSITION_X,
                                     slot->position_x);
            valuator_mask_set_double(mask, CMT_AXIS_MT_POSITION_Y,
                                     slot->position_y);
            valuator_mask_set_double(mask, CMT_AXIS_MT_PRESSURE,
                                     slot->pressure);
            valuator_mask_set_double(mask, CMT_AXIS_MT_TOUCH_MAJOR,
                                     slot->touch_major);
            valuator_mask_set_double(mask, CMT_AXIS_TOUCH_TIMESTAMP,
                                     StimeFromTimeval(tv));
            valuator_mask_set_double(mask, CMT_AXIS_X, slot->position_x);
            valuator_mask_set_double(mask, CMT_AXIS_Y, slot->position_y);

            if (rec->slot_states[i] == SLOT_STATUS_RAW) {
                xf86PostTouchEvent(dev, i, XI_TouchUpdate, 0, mask);
            } else {
                /* take over STATUS_GESTURE slots too */
                if (rec->slot_states[i] == SLOT_STATUS_GESTURE)
                    has_gesture_fingers = true;
                xf86PostTouchEvent(dev, i, XI_TouchBegin, 0, mask);

            }
            rec->slot_states[i] = SLOT_STATUS_RAW;
        }

        if (has_gesture_fingers) {
            /* push empty hardware state to clear interpreter state */
            hwstate.timestamp = StimeFromTimeval(tv);
            GestureInterpreterPushHardwareState(rec->interpreter, &hwstate);
        }
        return;
    }

    current_finger = 0;
    for (i = 0; i < evstate->slot_count; i++) {
        slot = &evstate->slots[i];
        if (slot->tracking_id == -1) {
            rec->slot_states[i] = SLOT_STATUS_FREE;
            continue;
        }

        if (rec->slot_states[i] == SLOT_STATUS_FREE) {
            rec->slot_states[i] = SLOT_STATUS_GESTURE;
        } else if (rec->slot_states[i] == SLOT_STATUS_RAW) {
            /* ignore any fingers that are still present from raw mode */
            continue;
        }

        rec->fingers[current_finger].touch_major = (float)slot->touch_major;
        rec->fingers[current_finger].touch_minor = (float)slot->touch_minor;
        rec->fingers[current_finger].width_major = (float)slot->width_major;
        rec->fingers[current_finger].width_minor = (float)slot->width_minor;
        rec->fingers[current_finger].pressure    = (float)slot->pressure;
        rec->fingers[current_finger].orientation = (float)slot->orientation;
        rec->fingers[current_finger].position_x  = (float)slot->position_x;
        rec->fingers[current_finger].position_y  = (float)slot->position_y;
        rec->fingers[current_finger].tracking_id = slot->tracking_id;
        current_finger++;
    }
    hwstate.timestamp = StimeFromTimeval(tv);

    for (i = 0; i < EVDEV_BUTTON_MAP_SIZE; ++i) {
        if (Event_Get_Button(evdev, kEvdevButtonMap[i][0]))
            hwstate.buttons_down |= kEvdevButtonMap[i][1];
    }

    hwstate.touch_cnt = Event_Get_Touch_Count(evdev);
    hwstate.finger_cnt = current_finger;
    hwstate.fingers = rec->fingers;
    hwstate.rel_x = evstate->rel_x;
    hwstate.rel_y = evstate->rel_y;
    hwstate.rel_wheel = evstate->rel_wheel;
    hwstate.rel_hwheel = evstate->rel_hwheel;
    GestureInterpreterPushHardwareState(rec->interpreter, &hwstate);
}

static void SetTimeValues(ValuatorMask* mask,
                          const struct Gesture* gesture,
                          DeviceIntPtr dev,
                          BOOL is_absolute)
{
    double start_time = gesture->start_time;
    double end_time = gesture->end_time;

    if (!is_absolute) {
        /*
         * We send the movement axes as relative values, which causes the
         * times to be sent as relative values too. This code computes the
         * right relative values.
         */
        start_time -= dev->last.valuators[CMT_AXIS_DBL_START_TIME];
        end_time -= dev->last.valuators[CMT_AXIS_DBL_END_TIME];
    }

    valuator_mask_set_double(mask, CMT_AXIS_DBL_START_TIME, start_time);
    valuator_mask_set_double(mask, CMT_AXIS_DBL_END_TIME, end_time);
}

static void SetOrdinalValues(ValuatorMask* mask,
                             DeviceIntPtr dev,
                             float x,
                             float y,
                             BOOL is_absolute) {
    if (!is_absolute) {
        /*
         * We send the movement axes as relative values, which causes the
         * times to be sent as relative values too. This code computes the
         * right relative values.
         */
        x -= dev->last.valuators[CMT_AXIS_ORDINAL_X];
        y -= dev->last.valuators[CMT_AXIS_ORDINAL_Y];
    }

    valuator_mask_set_double(mask, CMT_AXIS_ORDINAL_X, x);
    valuator_mask_set_double(mask, CMT_AXIS_ORDINAL_Y, y);
}

static void Gesture_Gesture_Ready(void* client_data,
                                  const struct Gesture* gesture)
{
    GesturePtr rec = client_data;
    DeviceIntPtr dev = rec->dev;
    ValuatorMask* mask = rec->mask;
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;

    if (cmt->props.raw_passthrough) {
        DBG(info, "Gesture Suppressed");
        return;
    }

    DBG(info, "Gesture Start: %f End: %f \n",
        gesture->start_time, gesture->end_time);

    valuator_mask_zero(mask);
    switch (gesture->type) {
        case kGestureTypeContactInitiated:
            /* TODO(adlr): handle contact initiated */
            break;
        case kGestureTypeMove: {
            const GestureMove* move = &gesture->details.move;
            DBG(info, "Gesture Move: (%f, %f) [%f, %f]\n",
                move->dx, move->dy, move->ordinal_dx, move->ordinal_dy);
            valuator_mask_set_double(mask, CMT_AXIS_X, move->dx);
            valuator_mask_set_double(mask, CMT_AXIS_Y, move->dy);
            SetTimeValues(mask, gesture, dev, FALSE);
            SetOrdinalValues(mask,
                             dev,
                             move->ordinal_dx,
                             move->ordinal_dy,
                             FALSE);
            xf86PostMotionEventM(dev, FALSE, mask);
            break;
        }
        case kGestureTypeScroll: {
            const GestureScroll* scroll = &gesture->details.scroll;
            DBG(info, "Gesture Scroll: (%f, %f) [%f, %f]\n",
                scroll->dx, scroll->dy, scroll->ordinal_dx, scroll->ordinal_dy);
            valuator_mask_set_double(mask, CMT_AXIS_SCROLL_X, scroll->dx);
            valuator_mask_set_double(mask, CMT_AXIS_SCROLL_Y, scroll->dy);
            valuator_mask_set_double(mask, CMT_AXIS_FINGER_COUNT, 2.0);
            SetTimeValues(mask, gesture, dev, TRUE);
            SetOrdinalValues(mask,
                             dev,
                             scroll->ordinal_dx,
                             scroll->ordinal_dy,
                             TRUE);
            xf86PostMotionEventM(dev, TRUE, mask);
            break;
        }
        case kGestureTypeButtonsChange: {
            const GestureButtonsChange* buttons = &gesture->details.buttons;
            DBG(info, "Gesture Button Change: down=0x%02x up=0x%02x\n",
                buttons->down, buttons->up);
            SetTimeValues(mask, gesture, dev, TRUE);
            if (buttons->down & GESTURES_BUTTON_LEFT)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_LEFT, 1, mask);
            if (buttons->down & GESTURES_BUTTON_MIDDLE)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_MIDDLE, 1, mask);
            if (buttons->down & GESTURES_BUTTON_RIGHT)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_RIGHT, 1, mask);
            if (buttons->down & GESTURES_BUTTON_BACK)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_BACK, 1, mask);
            if (buttons->down & GESTURES_BUTTON_FORWARD)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_FORWARD, 1, mask);
            if (buttons->up & GESTURES_BUTTON_LEFT)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_LEFT, 0, mask);
            if (buttons->up & GESTURES_BUTTON_MIDDLE)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_MIDDLE, 0, mask);
            if (buttons->up & GESTURES_BUTTON_RIGHT)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_RIGHT, 0, mask);
            if (buttons->up & GESTURES_BUTTON_BACK)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_BACK, 0, mask);
            if (buttons->up & GESTURES_BUTTON_FORWARD)
                xf86PostButtonEventM(dev, TRUE, CMT_BTN_FORWARD, 0, mask);
            break;
        }
        case kGestureTypeFling: {
            const GestureFling* fling = &gesture->details.fling;
            DBG(info, "Gesture Fling: (%f, %f) [%f, %f] fling_state=%d\n",
                fling->vx, fling->vy, fling->ordinal_vx, fling->ordinal_vy,
                fling->fling_state);
            valuator_mask_set_double(mask, CMT_AXIS_DBL_FLING_VX, fling->vx);
            valuator_mask_set_double(mask, CMT_AXIS_DBL_FLING_VY, fling->vy);
            valuator_mask_set(mask, CMT_AXIS_FLING_STATE, fling->fling_state);
            SetTimeValues(mask, gesture, dev, TRUE);
            SetOrdinalValues(mask,
                             dev,
                             fling->ordinal_vx,
                             fling->ordinal_vy,
                             TRUE);
            xf86PostMotionEventM(dev, TRUE, mask);
            break;
        }
        case kGestureTypeSwipe: {
            const GestureSwipe* swipe = &gesture->details.swipe;
            DBG(info, "Gesture Swipe: (%f, %f) [%f, %f]\n",
                swipe->dx, swipe->dy, swipe->ordinal_dx, swipe->ordinal_dy);
            valuator_mask_set_double(mask, CMT_AXIS_SCROLL_X, swipe->dx);
            valuator_mask_set_double(mask, CMT_AXIS_SCROLL_Y, swipe->dy);
            valuator_mask_set_double(mask, CMT_AXIS_FINGER_COUNT, 3.0);
            SetTimeValues(mask, gesture, dev, TRUE);
            SetOrdinalValues(mask,
                             dev,
                             swipe->ordinal_dx,
                             swipe->ordinal_dy,
                             TRUE);
            xf86PostMotionEventM(dev, TRUE, mask);
            break;
        }
        case kGestureTypeSwipeLift:
            DBG(info, "Gesture Swipe Lift\n");
            // Turn a swipe lift into a fling start.
            SetTimeValues(mask, gesture, dev, TRUE);
            valuator_mask_set_double(mask, CMT_AXIS_DBL_FLING_VX, 0);
            valuator_mask_set_double(mask, CMT_AXIS_DBL_FLING_VY, 0);
            valuator_mask_set(mask, CMT_AXIS_FLING_STATE, 0);
            xf86PostMotionEventM(dev, TRUE, mask);
            break;
        case kGestureTypePinch: {
            const GesturePinch* pinch = &gesture->details.pinch;
            DBG(info, "Gesture Pinch: dz=%f [%f]\n",
                pinch->dz, pinch->ordinal_dz);
            break;
        }
        case kGestureTypeMetrics: {
            const GestureMetrics* metrics = &gesture->details.metrics;
            DBG(info, "Gesture Metrics: [%f, %f] type=%d\n",
                metrics->data[0], metrics->data[1], metrics->type);
            valuator_mask_set_double(mask, CMT_AXIS_METRICS_DATA1,
                metrics->data[0]);
            valuator_mask_set_double(mask, CMT_AXIS_METRICS_DATA2,
                metrics->data[1]);
            valuator_mask_set(mask, CMT_AXIS_METRICS_TYPE, metrics->type);
            SetTimeValues(mask, gesture, dev, TRUE);
            xf86PostMotionEventM(dev, TRUE, mask);
            break;
        }
        default:
            ERR(info, "Unrecognized gesture type (%u)\n", gesture->type);
            break;
    }
}

static GesturesTimer*
Gesture_TimerCreate(void* provider_data)
{
    DeviceIntPtr dev = provider_data;
    InputInfoPtr info = dev->public.devicePrivate;
    CmtDevicePtr cmt = info->private;
    GesturesTimer* timer = (GesturesTimer*)calloc(1, sizeof(GesturesTimer));
    if (!timer)
        return NULL;
    timer->timer = TimerSet(NULL, 0, 0, NULL, 0);
    if (!timer->timer) {
        free(timer);
        return NULL;
    }
    timer->is_monotonic = cmt->evdev.info.is_monotonic;
    return timer;
}

static void
Gesture_TimerSet(void* provider_data,
                 GesturesTimer* timer,
                 stime_t delay,
                 GesturesTimerCallback callback,
                 void* callback_data)
{
    CARD32 ms = delay * 1000.0;

    if (!timer)
        return;
    timer->callback = callback;
    timer->callback_data = callback_data;
    if (ms == 0)
        ms = 1;
    TimerSet(timer->timer, 0, ms, Gesture_TimerCallback, timer);
}

static void
Gesture_TimerCancel(void* provider_data, GesturesTimer* timer)
{
    TimerCancel(timer->timer);
}

static void
Gesture_TimerFree(void* provider_data, GesturesTimer* timer)
{
    TimerFree(timer->timer);
    timer->timer = NULL;
    free(timer);
}

static CARD32
Gesture_TimerCallback(OsTimerPtr timer,
                      CARD32 millis,
                      pointer callback_data)
{
    GesturesTimer* tm = callback_data;
    stime_t now;
    stime_t rc;
    CARD32 next_timeout = 0;

    if (tm->is_monotonic) {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      now = StimeFromTimespec(&ts);
    } else {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      now = StimeFromTimeval(&tv);
    }

    rc = tm->callback(now, tm->callback_data);
    if (rc >= 0.0) {
        next_timeout = rc * 1000.0;
        if (next_timeout == 0)
            next_timeout = 1;
    }

    return next_timeout;
}

static enum GestureInterpreterDeviceClass
Gesture_Device_Class(EvdevClass cls) {
  switch (cls) {
    case EvdevClassMouse:
      return GESTURES_DEVCLASS_MOUSE;
    case EvdevClassMultitouchMouse:
      return GESTURES_DEVCLASS_MULTITOUCH_MOUSE;
    case EvdevClassTouchpad:
      return GESTURES_DEVCLASS_TOUCHPAD;
    case EvdevClassTouchscreen:
      return GESTURES_DEVCLASS_TOUCHSCREEN;
    default:
      return GESTURES_DEVCLASS_UNKNOWN;
  }
}

_X_EXPORT void gestures_log(int verb, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (verb > 0)
    xf86VDrvMsgVerb(-1, X_INFO, 7, fmt, args);
  else
    xf86VDrvMsgVerb(-1, X_ERROR, 0, fmt, args);
  va_end(args);
}
