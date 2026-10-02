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

#ifndef DM_PREPARED_RENDER_LIST_H
#define DM_PREPARED_RENDER_LIST_H

#include "render.h"
#include <dlib/array.h>

namespace dmRender
{
    // Internal mixed-component PoC. Build only while the consumer is idle.
    // Graphics handles remain protected by the engine's resource mutation barrier.
    typedef void (*RenderListBoundsFn)(const RenderListEntry& entry, dmVMath::Vector4* sphere);
    void SetRenderListSnapshotBounds(HRenderContext context, HRenderListDispatch dispatch, RenderListBoundsFn fn);

    struct PreparedRenderEntry
    {
        RenderListEntry m_Entry;
        RenderObject m_Object;
        dmVMath::Vector4 m_Sphere; // xyz center, squared radius; negative means unculled
    };

    struct PreparedRenderList
    {
        enum { MAX_ENTRIES = 4096, MAX_CONSTANT_BYTES = 4 * 1024 * 1024 };
        dmArray<PreparedRenderEntry> m_Entries;
        dmArray<HNamedConstantBuffer> m_Constants;
        uint64_t m_PrepareTime;
        uint64_t m_PrepareCount;
        PreparedRenderList();
        ~PreparedRenderList();
    };

    // Materializes geometry and constants on the producer, releases text layout
    // references there, and replaces all component pointers with owned packets.
    // keep_other_dispatches materializes the admitted GUI/particle dispatches in
    // place for the inline control, preserving sprite snapshot entries.
    bool PrepareRenderList(HRenderContext context, PreparedRenderList* output, bool keep_other_dispatches = false);
    void SubmitPreparedRenderList(HRenderContext context, PreparedRenderList* frame);
    uint64_t GetPreparedRenderListCapacity(const PreparedRenderList* frame);
}
#endif
