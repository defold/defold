#include "physics_debug_render.h"

#include <ddf/ddf.h>
#include <render/render.h>
#include <render/mesh_ddf.h>
#include <render_debug/debugrenderer.h>

namespace PhysicsDebugRender
{
    void RenderLine(void* ctx, Vectormath::Aos::Point3 p0, Vectormath::Aos::Point3 p1, Vectormath::Aos::Vector4 color)
    {
        dmRender::RenderContext* context = (dmRender::RenderContext*)ctx;
        dmRenderDebug::Line(context->m_ViewProj, p0, p1, color);
    }
}
