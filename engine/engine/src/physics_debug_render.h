#ifndef GAME_PHYSICS_DEBUG_RENDER_H
#define GAME_PHYSICS_DEBUG_RENDER_H

#include <stdint.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>

namespace PhysicsDebugRender
{
    void DrawLines(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data);
    void DrawTriangles(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data);
}

#endif
