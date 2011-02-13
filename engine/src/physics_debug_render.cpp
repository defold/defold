#include "physics_debug_render.h"

#include <ddf/ddf.h>
#include <render/render.h>

namespace PhysicsDebugRender
{
    void RenderLine(void* context, Vectormath::Aos::Point3 p0, Vectormath::Aos::Point3 p1, Vectormath::Aos::Vector4 color)
    {
        dmRender::Line3D((dmRender::HRenderContext)context, p0, p1, color, color);
    }
}
