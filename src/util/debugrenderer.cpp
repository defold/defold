#include "debugrenderer.h"
#include "graphics_device.h"


using namespace Vectormath::Aos;

namespace GFXUtil
{
    void DebugRendererCube(float size, Vector3 position)
    {

        GFXHContext context = GFXGetContext();

        Vector4 v(1.0, 1.0, 1.0, 0.0);
        GFXSetFragmentConstant(context, &v, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(position);

        GFXSetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);


        GFXSetVertexStream(context, 0, 3, GFX_TYPE_FLOAT, 0, (void*) &CubeVertices[0]);
        GFXDisableVertexStream(context, 1);
        GFXDisableVertexStream(context, 2);

        GFXDrawElements(context, GFX_PRIMITIVE_TRIANGLES, 3*12, GFX_TYPE_UNSIGNED_INT, (void*) &CubeIndices[0]);

    }

}
