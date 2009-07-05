#ifndef __DEBUGRENDERER_H__
#define __DEBUGRENDERER_H__

#include <vectormath/cpp/vectormath_aos.h>



namespace dmRenderDebug
{

    static const float CubeVertices[] = {
            -1, -1, -1,
            1, -1, -1,
            1, 1, -1,
            -1, 1, -1,
            -1, -1, 1,
            1, -1, 1,
            1, 1, 1,
            -1, 1, 1};

    static const int CubeIndices[] =
    {
            0, 1, 2,
            2, 3, 0,
            0, 3, 7,
            7, 4, 0,
            4, 7, 6,
            6, 5, 4,
            1, 5, 6,
            6, 2, 1,
            0, 4, 5,
            5, 1, 0,
            3, 2, 6,
            6, 7, 3
    };

    void RenderCube(Vectormath::Aos::Point3 position, float size);
}

#endif // __DEBUGRENDERER_H__
