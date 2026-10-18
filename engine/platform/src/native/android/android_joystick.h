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

#ifndef _ANDROID_JOYSTICK_H_
#define _ANDROID_JOYSTICK_H_

#define NATIVE_ANDROID_GAMEPAD_DISCONNECTED   0
#define NATIVE_ANDROID_GAMEPAD_CONNECTED      1
#define NATIVE_ANDROID_GAMEPAD_REFRESHING     2

// SDL reserves the last four Android GUID bytes for button and axis capabilities.
static inline void dmNativeAndroidCreateJoystickDeviceGuid(unsigned short vendor, unsigned short product, const char* descriptor,
                                                           unsigned int button_mask, unsigned int axis_mask, char guid[33])
{
    static const char hex[] = "0123456789abcdef";
    unsigned char capabilities[4] = { (unsigned char)button_mask, (unsigned char)(button_mask >> 8),
                                      (unsigned char)axis_mask, (unsigned char)(axis_mask >> 8) };
    dmNativeCreateJoystickDeviceGuid(0x0005, vendor, product, 0, 0, descriptor, 0, 0, guid);
    for (int i = 0; i < 4; ++i)
    {
        guid[24 + i * 2] = hex[capabilities[i] >> 4];
        guid[25 + i * 2] = hex[capabilities[i] & 0x0f];
    }
}

// Return nonzero for a handled gamepad key to suppress Android navigation fallback events.
int dmNativeAndroidUpdateJoystick(const AInputEvent* event);
void dmNativeAndroidDiscoverJoysticks();
void dmNativeAndroidSetJoystickLayoutLegacy(int joy, int legacy);
void dmNativeAndroidInitializeJoystickAxes(int joy, const float* axes, int num_axes);

#endif // _ANDROID_JOYSTICK_H_
