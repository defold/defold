#ifndef DM_RENDER_DEBUG_RENDERER_H
#define DM_RENDER_DEBUG_RENDERER_H_

#include <vectormath/cpp/vectormath_aos.h>

#include "render.h"

namespace dmRender
{
    /**
     * Initialize debug render system
     * @param renderworld Global renderworld to add entities to
     */
    void InitializeDebugRenderer(HRenderContext render_context, const void* vp_data, uint32_t vp_data_size, const void* fp_data, uint32_t fp_data_size);

    /**
     * Finalize debug render system
     */
    void FinalizeDebugRenderer(HRenderContext render_context);

    void ClearDebugRenderObjects(HRenderContext render_context);

    void FlushDebug(HRenderContext render_context);
}

#endif // DM_RENDER_DEBUG_RENDERER_H
