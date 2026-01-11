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

#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#define DM_PLATFORM_NAME_LINUX   "linux"
#define DM_PLATFORM_NAME_MACOS   "macos"
#define DM_PLATFORM_NAME_WINDOWS "windows"
#define DM_PLATFORM_NAME_WEB     "web"
#define DM_PLATFORM_NAME_ANDROID "android"
#define DM_PLATFORM_NAME_IOS 	 "ios"
#define DM_PLATFORM_NAME_SWITCH  "switch"
#define DM_PLATFORM_NAME_PLAYSTATION "playstation"

// Note: DM_PLATFORM is used as a key in data files, e.g. ".gamepads"

#if defined(ANDROID)
#define DM_PLATFORM DM_PLATFORM_NAME_ANDROID
#elif defined(__linux__)
#define DM_PLATFORM DM_PLATFORM_NAME_LINUX
#elif defined(DM_PLATFORM_IOS)
#define DM_PLATFORM DM_PLATFORM_NAME_IOS
#elif defined(DM_PLATFORM_MACOS)
#define DM_PLATFORM DM_PLATFORM_NAME_MACOS
#elif defined(_WIN32)
#define DM_PLATFORM DM_PLATFORM_NAME_WINDOWS
#elif defined(__EMSCRIPTEN__)
#define DM_PLATFORM DM_PLATFORM_NAME_WEB
#elif defined(__NX__)
#define DM_PLATFORM DM_PLATFORM_NAME_SWITCH
#elif defined(__SCE__)
#define DM_PLATFORM DM_PLATFORM_NAME_PLAYSTATION
#else
#error "Unsupported platform"
#endif

#endif
