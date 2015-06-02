#ifndef DM_RENDER_DEBUG_RENDERER_H
#define DM_RENDER_DEBUG_RENDERER_H

#include <vectormath/cpp/vectormath_aos.h>

#include "render.h"

namespace dmRender
{
    /**
     * Initialize debug render system
     * @param render_context Render context
     * @param max_vertex_count Max vertex count (per type)
     * @param vp_data Vertex program
     * @param vp_data_size Vertex progrm size
     * @param fp_data Fragment program
     * @param fp_data_size Fragment program size
     */
    void InitializeDebugRenderer(HRenderContext render_context, uint32_t max_vertex_count, const void* vp_data, uint32_t vp_data_size, const void* fp_data, uint32_t fp_data_size);

    /**
     * Finalize debug render system
     */
    void FinalizeDebugRenderer(HRenderContext render_context);

    void ClearDebugRenderObjects(HRenderContext render_context);

    void FlushDebug(HRenderContext render_context, uint32_t render_order);
}

#endif // DM_RENDER_DEBUG_RENDERER_H
