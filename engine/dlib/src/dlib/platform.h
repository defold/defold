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

#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#define DM_PLATFORM_LINUX   "linux"
#define DM_PLATFORM_OSX     "osx"
#define DM_PLATFORM_WINDOWS "windows"
#define DM_PLATFORM_WEB     "web"
#define DM_PLATFORM_ANDROID "android"
#define DM_PLATFORM_IOS 	"ios"
#define DM_PLATFORM_SWITCH 	"switch"

// Note: DM_PLATFORM is used as a key in data files, e.g. ".gamepads"

#if defined(ANDROID)
#define DM_PLATFORM DM_PLATFORM_ANDROID
#elif defined(__linux__)
#define DM_PLATFORM DM_PLATFORM_LINUX
#elif defined(__MACH__) && (defined(__arm__) || defined(__arm64__))
#define DM_PLATFORM DM_PLATFORM_IOS
#elif defined(__MACH__)
#define DM_PLATFORM DM_PLATFORM_OSX
#elif defined(_WIN32)
#define DM_PLATFORM DM_PLATFORM_WINDOWS
#elif defined(__EMSCRIPTEN__)
#define DM_PLATFORM DM_PLATFORM_WEB
#elif defined(__NX__)
#define DM_PLATFORM DM_PLATFORM_SWITCH
#else
#error "Unsupported platform"
#endif

#endif
