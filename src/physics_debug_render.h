#ifndef GAME_PHYSICS_DEBUG_RENDER_H
#define GAME_PHYSICS_DEBUG_RENDER_H

#include <vectormath/cpp/vectormath_aos.h>

namespace PhysicsDebugRender
{
    void RenderLine(void* ctx, Vectormath::Aos::Point3 p0, Vectormath::Aos::Point3 p1, Vectormath::Aos::Vector4 color);
}

#endif
