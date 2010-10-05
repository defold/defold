#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/array.h>
#include <graphics/graphics_device.h>
#include "fontrenderer.h"
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypetext.h"


namespace dmRender
{
    using namespace Vectormath::Aos;


    void RenderTypeTextSetup(const RenderContext* rendercontext)
    {
        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);
    }

    void RenderTypeTextDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count)
    {
        RenderObject* ro = (RenderObject*)*ro_;


        HFont font = (HFont)ro->m_Data;
        dmArray<SFontVertex>* vertex_data = (dmArray<SFontVertex>*)ro->m_Go;

        if (!font || !vertex_data || vertex_data->Size() == 0)
            return;



        SFontVertex* data = &vertex_data->Front();
        float* pos = data->m_Position;
        float* colour = data->m_Colour;
        float* uv = data->m_UV;


        dmGraphics::HContext context = rendercontext->m_GFXContext;
        Matrix4 ident = Matrix4::identity();

        dmGraphics::SetVertexProgram(context, GetVertexProgram(font));
        dmGraphics::SetFragmentProgram(context, GetFragmentProgram(font));

        Matrix4 mat = Matrix4::orthographic( 0, 960, 540, 0, 10, -10 );

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&ident, 4, 4);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) pos);

        dmGraphics::SetVertexStream(context, 1, 2, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) uv);

        dmGraphics::SetVertexStream(context, 2, 4, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) colour);

        dmGraphics::SetTexture(context, GetTexture(font));

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, vertex_data->Size());

        dmGraphics::EnableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::DisableState(context, dmGraphics::BLEND);

        dmGraphics::DisableVertexStream(context, 0);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        vertex_data->SetSize(0);
    }

}
