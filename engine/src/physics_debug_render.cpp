#include "physics_debug_render.h"

#include <ddf/ddf.h>
#include <render/render.h>

namespace PhysicsDebugRender
{
    void DrawLines(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data)
    {
        for (uint32_t i = 0; i < point_count/2; ++i)
            dmRender::Line3D((dmRender::HRenderContext)user_data, points[2*i], points[2*i+1], color, color);
    }
}
