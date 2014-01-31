// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include <xf86.h>
#include <xf86Xinput.h>

// Provide these symbols for unittests

int GetMotionHistorySize(void) {
  return 0;
}

Bool InitPointerDeviceStruct(DevicePtr device,
                             CARD8* map,
                             int numButtons,
                             Atom* btn_labels,
                             PtrCtrlProcPtr controlProc,
                             int numMotionEvents,
                             int numAxes,
                             Atom* axes_labels) {
  return 0;
}

Atom MakeAtom(const char* string,
              unsigned len,
              Bool makeit) {
  return 0;
}

const char* NameForAtom(Atom atom) {
  return "";
}

void TimerCancel(OsTimerPtr  pTimer) {
  return;
}

void TimerFree(OsTimerPtr  pTimer) {
  return;
}

OsTimerPtr TimerSet(OsTimerPtr timer,
                    int flags,
                    CARD32 millis,
                    OsTimerCallback func,
                    pointer arg) {
  return timer;
}

void xf86AddEnabledDevice(InputInfoPtr pInfo) {
  return;
}

void xf86AddInputDriver(InputDriverPtr driver, pointer module, int flags) {
  return;
}

InputInfoPtr xf86AllocateInput(InputDriverPtr drv, int flags) {
  return NULL;
}

int xf86BlockSIGIO(void) {
  return 0;
}

char* xf86CheckStrOption(pointer optlist, const char* name, char* deflt) {
  return deflt;
}

void xf86CollectInputOptions(InputInfoPtr pInfo,
                             const char** defaultOpts, pointer extraOpts) {
  return;
}

void xf86DeleteInput(InputInfoPtr pInp, int flags) {
  return;
}

int xf86FlushInput(int fd) {
  return 0;
}

void xf86IDrvMsg(LocalDevicePtr dev, MessageType type,
                 const char* format, ...) {
  va_list args;
  va_start(args, format);
  xf86VIDrvMsgVerb(dev, type, -1, format, args);
  va_end(args);
}

void xf86IDrvMsgVerb(LocalDevicePtr dev, MessageType type, int verb,
                     const char* format, ...) {
  va_list args;
  va_start(args, format);
  xf86VIDrvMsgVerb(dev, type, verb, format, args);
  va_end(args);
}

void xf86InitValuatorAxisStruct(DeviceIntPtr dev, int axnum, Atom label,
                                int minval, int maxval, int resolution,
                                int min_res, int max_res) {
  return;
}

void xf86InitValuatorDefaults(DeviceIntPtr dev, int axnum) {
  return;
}

void xf86MsgVerb(int unused1, int unused2, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
}

void xf86PostButtonEvent(DeviceIntPtr device, int is_absolute, int button,
                         int is_down, int first_valuator,
                         int num_valuators, ...) {
  printf("PostButtonEvent: %d %d %d %d\n",
         is_absolute, button, is_down, first_valuator, num_valuators);
}

void xf86PostMotionEvent(DeviceIntPtr device, int is_absolute,
                         int first_valuator, int num_valuators, ...) {
  printf("PostMotionEvent: %d %d %d %d\n",
         is_absolute, first_valuator, num_valuators);
}

void xf86ProcessCommonOptions(InputInfoPtr pInfo, pointer options) {
  return;
}

void xf86RemoveEnabledDevice(InputInfoPtr pInfo) {
  return;
}

int xf86SetBoolOption(pointer list, const char* name, int deflt) {
  return deflt;
}

int xf86SetIntOption(pointer optlist, const char* name, int deflt) {
  return deflt;
}

double xf86SetRealOption(pointer optlist, const char* name, double deflt) {
  return deflt;
}

char* xf86SetStrOption(pointer optlist, const char* name, char* deflt) {
  return deflt;
}

void xf86UnblockSIGIO(int s) {
  return;
}

void xf86VIDrvMsgVerb(LocalDevicePtr dev, MessageType type, int verb,
                      const char* format, va_list args) {
  vprintf(format, args);
}

int XIChangeDeviceProperty(DeviceIntPtr dev,
                           Atom property,
                           Atom type,
                           int format,
                           int mode,
                           unsigned long len,
                           pointer value,
                           Bool sendevent) {
  return 0;
}

int XIDeleteDeviceProperty(DeviceIntPtr device,
                           Atom property,
                           Bool fromClient) {
  return 0;
}

Atom XIGetKnownProperty(char* name) {
  return 0;
}

long XIRegisterPropertyHandler(
    DeviceIntPtr dev,
    int (*SetProperty) (DeviceIntPtr dev,
                        Atom property,
                        XIPropertyValuePtr prop,
                        BOOL checkonly),
    int (*GetProperty) (DeviceIntPtr dev,
                        Atom property),
    int (*DeleteProperty) (DeviceIntPtr dev,
                           Atom property)) {
  return 0;
}

int XISetDevicePropertyDeletable(DeviceIntPtr dev,
                                 Atom property,
                                 Bool deletablekF2) {
  return 0;
}

void XIUnregisterPropertyHandler(DeviceIntPtr dev, long id) {
  return;
}
