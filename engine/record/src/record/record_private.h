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

#ifndef DM_RECORD_PRIVATE_H
#define DM_RECORD_PRIVATE_H

#include "record.h"

namespace dmRecord
{
    struct Window
    {
        uintptr_t m_Handle;
        double    m_X, m_Y, m_Width, m_Height, m_Scale;
    };

    // Snapshot window geometry on the main thread before handing it to the worker.
    bool GetWindow(void* native_window, Window* window);
    void GetNativeStatus(Status* status);
    bool StartNative(const Window* window, const char* path, uint32_t width, uint32_t height, uint32_t fps, bool audio, Status* status);
    bool StopNative(Status* status);
    void FinalizeNative();
} // namespace dmRecord

#endif
