#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypedebugprim.h"


namespace dmRenderDebug
{
    using namespace Vectormath::Aos;

    static const float CubeVertices[] =
    {
            -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1
    };

    static const int CubeIndices[] =
    {
            0, 1, 2, 2, 3, 0, 0, 3, 7, 7, 4, 0, 4, 7, 6, 6, 5, 4, 1, 5, 6, 6, 2, 1, 0, 4, 5, 5, 1, 0, 3, 2, 6, 6, 7, 3
    };

    static void SetupMatrices(dmGraphics::HContext context, Matrix4* view_proj, Matrix4* mat)
    {
        if (view_proj) dmGraphics::SetVertexConstantBlock(context, (const Vector4*)view_proj, 0, 4);
        if (mat)       dmGraphics::SetVertexConstantBlock(context, (const Vector4*)mat, 4, 4);
    }


    void RenderTypeDebugPrimSetup(const dmRender::RenderContext* rendercontext)
    {
        dmGraphics::HContext context = rendercontext->m_GFXContext;

//        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
//        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
//        dmGraphics::EnableState(context, dmGraphics::BLEND);


    }

    void RenderTypeDebugPrimDraw(const dmRender::RenderContext* rendercontext, const dmRender::HRenderObject* ro, uint32_t count)
    {
        DebugRenderInfo* info = (DebugRenderInfo*)dmRender::GetData(ro[0]);

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        switch (info->m_Type)
        {
            case DEBUG_RENDER_TYPE_SQUARE:
            {
#if 1
                dmGraphics::SetFragmentProgram(context, (dmGraphics::HFragmentProgram)info->m_Data4);
                dmGraphics::SetVertexProgram(context, (dmGraphics::HVertexProgram)info->m_Data5);

                Point3* position = (Point3*)info->m_Data0;
                Vector3* size = (Vector3*)info->m_Data1;
                Vector4* color = (Vector4*)info->m_Data2;


                dmGraphics::SetFragmentConstant(context, color, 0);
                size->setZ(0.0f);
                Matrix4 mat = Matrix4::scale(*size*0.5f);
                mat.setTranslation(Vector3(*position) );

                dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
                dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);

//                SetupMatrices(context, &view_proj, &mat);

                dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) CubeVertices);
                dmGraphics::DisableVertexStream(context, 1);
                dmGraphics::DisableVertexStream(context, 2);

                dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) CubeIndices);
#endif
                break;
            }

            case DEBUG_RENDER_TYPE_LINES:
            {
                Point3* vertices = (Point3*)info->m_Data0;
                int* vertex_count = (int*)info->m_Data1;
                Vector4* color = (Vector4*)info->m_Data2;

                dmGraphics::SetFragmentConstant(context, color, 0);

                Matrix4 mat = Matrix4::identity();

//                SetupMatrices(context, &view_proj, &mat);

                dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, 0, (void*)vertices);
                dmGraphics::DisableVertexStream(context, 1);
                dmGraphics::DisableVertexStream(context, 2);

                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINE_STRIP, 0, *vertex_count);
                break;
            }

            default:
            {

            }
        }

    }

}
