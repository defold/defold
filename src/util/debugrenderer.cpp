#include "debugrenderer.h"
#include "graphics_device.h"


using namespace Vectormath::Aos;

namespace GFXUtil
{
    void DebugRendererCube(float size, Vector3 position)
    {

        Graphics::HContext context = Graphics::GetContext();

        Vector4 v(1.0, 1.0, 1.0, 0.0);
        Graphics::SetFragmentConstant(context, &v, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(position);

        Graphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);


        Graphics::SetVertexStream(context, 0, 3, Graphics::TYPE_FLOAT, 0, (void*) &CubeVertices[0]);
        Graphics::DisableVertexStream(context, 1);
        Graphics::DisableVertexStream(context, 2);

        Graphics::DrawElements(context, Graphics::PRIMITIVE_TRIANGLES, 3*12, Graphics::TYPE_UNSIGNED_INT, (void*) &CubeIndices[0]);

    }

}
