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

#ifndef DM_THREAD_H
#define DM_THREAD_H

#include <dmsdk/dlib/thread.h>

#if !defined(__EMSCRIPTEN__)
    #define DM_HAS_THREADS
#endif

#if defined(DM_USE_SINGLE_THREAD)
    #if defined(DM_HAS_THREADS)
        #undef DM_HAS_THREADS
    #endif
#endif

namespace dmThread
{
    /*# check for threading support
     *
     * @name dmThread::PlatformHasThreadSupport
     * @return true if platform has support for threads
     */
    static inline bool PlatformHasThreadSupport()
    {
    #if defined(DM_HAS_THREADS)
        return true;
    #else
        return false;
    #endif
    }
}

#endif // DM_THREAD_H
