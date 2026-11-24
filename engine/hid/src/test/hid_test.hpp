// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_HID_TEST_HPP
#define DM_HID_TEST_HPP

#include <hid/hid.h>

// Test support for injecting device state without exposing HID's private layout.
// Handles are borrowed; these functions run on the thread driving the test context.
namespace dmHID
{
    void TestSetDeviceIgnored(HContext context, InputSource source, bool ignored);
    void TestSetMouseLeftButtonFromTouch(HMouse mouse, bool from_touch);
    void TestSetMousePositionFromTouch(HMouse mouse, bool from_touch);
    void TestSetGamepadConnected(HGamepad gamepad, bool connected);
    void TestSetAccelerometerConnected(HContext context, bool connected);
    void TestSetAcceleration(HContext context, float x, float y, float z);
}

#endif // DM_HID_TEST_HPP
