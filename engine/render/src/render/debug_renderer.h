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

#ifndef DM_RENDER_DEBUG_RENDERER_H
#define DM_RENDER_DEBUG_RENDERER_H

#include "render.h"

namespace dmRender
{
    /**
     * Initialize debug render system
     * @param render_context Render context
     * @param max_vertex_count Max vertex count (per type)
     * @param sp_desc ShaderProgram shader desc
     * @param sp_desc_size ShaderProgram shader desc size
     */
	void InitializeDebugRenderer(HRenderContext render_context, uint32_t max_vertex_count, const void* sp_desc, uint32_t sp_desc_size);

    /**
     * Finalize debug render system
     */
    void FinalizeDebugRenderer(HRenderContext render_context);

    void ClearDebugRenderObjects(HRenderContext render_context);

    void FlushDebug(HRenderContext render_context, uint32_t render_order);
}

#endif // DM_RENDER_DEBUG_RENDERER_H
