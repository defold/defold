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

#ifndef DM_RENDER_FRAME_TEST_ALLOCATIONS_H
#define DM_RENDER_FRAME_TEST_ALLOCATIONS_H

#include <stdlib.h>
#include <dlib/memory.h>

extern "C" void* RenderFrameTestMalloc(size_t size);

namespace dmMemory
{
    Result RenderFrameTestAlignedMalloc(void** memory, unsigned int alignment, unsigned int size);
}

// Included only when compiling storage for test_render_frame. Read the real allocator
// declarations first so replacement functions do not inherit system DLL-import attributes.
#define malloc RenderFrameTestMalloc
#define AlignedMalloc RenderFrameTestAlignedMalloc

#endif // DM_RENDER_FRAME_TEST_ALLOCATIONS_H
