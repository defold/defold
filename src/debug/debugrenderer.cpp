#include "debugrenderer.h"
#include "graphics/graphics_device.h"


using namespace Vectormath::Aos;

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

    void RenderCube(Point3 position, float size)
    {

        dmGraphics::HContext context = dmGraphics::GetContext();

        Vector4 v(1.0, 1.0, 1.0, 0.0);
        dmGraphics::SetFragmentConstant(context, &v, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(Vector3(position) );

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);


        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &CubeVertices[0]);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) &CubeIndices[0]);

    }

}
