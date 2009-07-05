#include "debugrenderer.h"
#include "graphics_device.h"


using namespace Vectormath::Aos;

namespace GFXUtil
{
    void DebugRendererCube(float size, Vector3 position)
    {

        dmGraphics::HContext context = dmGraphics::GetContext();

        Vector4 v(1.0, 1.0, 1.0, 0.0);
        dmGraphics::SetFragmentConstant(context, &v, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(position);

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);


        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &CubeVertices[0]);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) &CubeIndices[0]);

    }

}
