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

#ifndef DM_WINDOW_DESKTOP_KEYS_H
#define DM_WINDOW_DESKTOP_KEYS_H

#include "platform_window_constants.h"

namespace dmPlatform
{
    // Additional desktop window keys without a corresponding HID key.
    enum DesktopKey
    {
        PLATFORM_KEY_NON_US_1     = 161,
        PLATFORM_KEY_NON_US_2     = 162,
        PLATFORM_KEY_PRINT_SCREEN = 283,
        PLATFORM_KEY_F13          = 302,
        PLATFORM_KEY_F14          = 303,
        PLATFORM_KEY_F15          = 304,
        PLATFORM_KEY_F16          = 305,
        PLATFORM_KEY_F17          = 306,
        PLATFORM_KEY_F18          = 307,
        PLATFORM_KEY_F19          = 308,
        PLATFORM_KEY_F20          = 309,
    };
}

#endif // DM_WINDOW_DESKTOP_KEYS_H
