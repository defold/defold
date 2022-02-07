// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_HID_NATIVE_DEFINES_H
#define DMSDK_HID_NATIVE_DEFINES_H

#include <stdint.h>
#if defined(__NX__)
#include <dmsdk/hid/nx64/hid_native_defines.h>
#else
#include <dmsdk/hid/glfw/hid_native_defines.h>
#endif

#endif // DMSDK_HID_NATIVE_DEFINES_H
