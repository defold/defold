#include "debugrenderer.h"
#include "graphics_device.h"


using namespace Vectormath::Aos;

namespace GFXUtil
{
    void DebugRendererCube(float size, Vector3 position)
    {

        GFX::HContext context = GFX::GetContext();

        Vector4 v(1.0, 1.0, 1.0, 0.0);
        GFX::SetFragmentConstant(context, &v, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(position);

        GFX::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);


        GFX::SetVertexStream(context, 0, 3, GFX::TYPE_FLOAT, 0, (void*) &CubeVertices[0]);
        GFX::DisableVertexStream(context, 1);
        GFX::DisableVertexStream(context, 2);

        GFX::DrawElements(context, GFX::PRIMITIVE_TRIANGLES, 3*12, GFX::TYPE_UNSIGNED_INT, (void*) &CubeIndices[0]);

    }

}
