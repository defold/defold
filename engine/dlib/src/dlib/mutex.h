// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_MUTEX_H
#define DM_MUTEX_H

#include <dmsdk/dlib/mutex.h> // the api + typedef

#if defined(DM_PLATFORM_VENDOR)
    #include <dlib/mutex_vendor.h>

#elif defined(_WIN32)
    #include <dlib/win32/mutex.h>

#elif defined(__linux__) || defined(__MACH__)
    #include <dlib/mutex_posix.h>
#elif defined (__EMSCRIPTEN__)
    #if defined(DM_NO_THREAD_SUPPORT)
        #include <dlib/mutex_empty.h>
    #else
        #include <dlib/mutex_posix.h>
    #endif
#else
    #error "Unsupported platform"
#endif

#endif // DM_MUTEX_H
