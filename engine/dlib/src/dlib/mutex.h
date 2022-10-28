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

#ifndef DM_MUTEX_H
#define DM_MUTEX_H

#include <dmsdk/dlib/mutex.h> // the api + typedef

#if defined(__SCE__)
#include <dlib/ps4/mutex.h>

#elif defined(__NX64__)
#include <dlib/nx64/mutex.h>

#elif defined(_WIN32)
#include <dlib/win32/mutex.h>

#elif defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)

#include <pthread.h>

namespace dmMutex
{
    struct Mutex
    {
        pthread_mutex_t  m_NativeHandle;
    };
}

#else
#error "Unsupported platform"
#endif

#endif // DM_MUTEX_H
