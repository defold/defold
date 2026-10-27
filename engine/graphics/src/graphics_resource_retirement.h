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

#ifndef DM_GRAPHICS_RESOURCE_RETIREMENT_H
#define DM_GRAPHICS_RESOURCE_RETIREMENT_H

#include <dlib/array.h>
#include <dlib/mutex.h>

namespace dmGraphics
{
    // Producers must not append to a list owned by an in-flight completion callback.
    template <typename T>
    struct ResourceRetirementQueue
    {
        dmArray<T>      m_Pending;
        dmMutex::HMutex m_Mutex;

        ResourceRetirementQueue()
        {
            m_Mutex = dmMutex::New();
        }

        ~ResourceRetirementQueue()
        {
            assert(m_Pending.Empty());
            dmMutex::Delete(m_Mutex);
        }

        void Push(const T& resource)
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            if (m_Pending.Full())
            {
                m_Pending.OffsetCapacity(8);
            }
            m_Pending.Push(resource);
        }

        // Call only after acquiring a free frame slot, or after all GPU work at shutdown.
        // The consumer exclusively owns the returned snapshot until it has drained it.
        void TakePending(dmArray<T>& submitted)
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            assert(submitted.Empty());
            m_Pending.Swap(submitted);
        }
    };
}

#endif
