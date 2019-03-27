#ifndef DM_RENDER_DEBUG_RENDERER_H
#define DM_RENDER_DEBUG_RENDERER_H

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include "render.h"

namespace dmRender
{
    /**
     * Initialize debug render system
     * @param render_context Render context
     * @param max_vertex_count Max vertex count (per type)
     * @param vp_desc VertexProgram shader desc
     * @param vp_desc_size VertexProgram shader desc size
     * @param fp_desc FragmentProgram shader desc
     * @param fp_desc_size FragmentProgram shader desc size
     */
	void InitializeDebugRenderer(HRenderContext render_context, uint32_t max_vertex_count, const void* vp_desc, uint32_t vp_desc_size, const void* fp_desc, uint32_t fp_desc_size);

    /**
     * Finalize debug render system
     */
    void FinalizeDebugRenderer(HRenderContext render_context);

    void ClearDebugRenderObjects(HRenderContext render_context);

    void FlushDebug(HRenderContext render_context, uint32_t render_order);
}

#endif // DM_RENDER_DEBUG_RENDERER_H
