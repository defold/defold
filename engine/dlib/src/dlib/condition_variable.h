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

#ifndef DM_CONDITION_VARIABLE_H
#define DM_CONDITION_VARIABLE_H

#include <stdint.h>
#include <dmsdk/dlib/condition_variable.h>

// Internal timed waits are available on the native desktop/mobile implementations.
// Other platform implementations retain the existing, untimed SDK contract.
#if defined(__linux__) || defined(__MACH__) || defined(_WIN32)
#define DM_HAS_CONDITION_VARIABLE_TIMEOUT 1
namespace dmConditionVariable
{
    // The caller holds mutex. Returns false on timeout; notifications (including
    // spurious wakeups) return true. Always recheck the predicate under mutex.
    bool WaitTimeout(HConditionVariable condition, dmMutex::HMutex mutex, uint32_t timeout_ms);
}
#endif

#endif // #ifndef DM_CONDITION_VARIABLE_H
