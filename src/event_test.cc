// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

extern "C" {
#undef __cplusplus
#define bool bool_
#define class class_
#define delete delete_
#define new new_
#define private private_
#define public public_
#include "libevdev_cmt.h"
#include "libevdev_event.h"
#undef bool
#undef class
#undef delete
#undef new
#undef private
#undef public
#define __cplusplus 1
}

class EventTest : public ::testing::Test {};

TEST(EventTest, Event_Get_LeftTest) {
  CmtDeviceRec cmt;
  InputInfoRec info;
  info.private_ = &cmt;
  cmt.absinfo[ABS_X].minimum = 123;

  EXPECT_EQ(123, Event_Get_Left(&info));
}
