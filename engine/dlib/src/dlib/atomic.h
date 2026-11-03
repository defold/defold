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

#ifndef DM_ATOMIC_H
#define DM_ATOMIC_H

#include <dmsdk/dlib/atomic.h>

// Internal publication helpers. A release store publishes preceding writes to a
// thread that observes the stored value with an acquire load. MSVC retains the
// stronger Interlocked operations used by the SDK helpers.
inline void dmAtomicStoreRelease32(int32_atomic_t* ptr, int32_t value)
{
#if defined(_MSC_VER)
    dmAtomicStore32(ptr, value);
#else
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
#endif
}

inline int32_t dmAtomicLoadAcquire32(int32_atomic_t* ptr)
{
#if defined(_MSC_VER)
    return dmAtomicGet32(ptr);
#else
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#endif
}

#endif //DM_ATOMIC_H
