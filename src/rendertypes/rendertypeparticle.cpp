#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/array.h>
#include <graphics/graphics_device.h>
#include "fontrenderer.h"
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypeparticle.h"


namespace dmRender
{
    using namespace Vectormath::Aos;


    void RenderTypeParticleSetup(const RenderContext* rendercontext)
    {
        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);
    }

    void RenderTypeParticleDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count)
    {
        RenderObject* ro = (RenderObject*)*ro_;

        SParticleRenderData* renderdata = (SParticleRenderData*)ro->m_Data;

        if (renderdata->m_VertexCount == 0 || renderdata->m_VertexData == 0)
            return;


        dmGraphics::HContext gfx_context = rendercontext->m_GFXContext;

        dmGraphics::SetBlendFunc(gfx_context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(gfx_context, dmGraphics::BLEND);

        dmGraphics::SetDepthMask(gfx_context, 0);

        float* pos = &renderdata->m_VertexData[0];
        float* uvs = &renderdata->m_VertexData[3];
        float* alpha = &renderdata->m_VertexData[5];


        // positions
        dmGraphics::SetVertexStream(gfx_context, 0, 3, dmGraphics::TYPE_FLOAT, renderdata->m_VertexStride, (void*)pos);
        // uv's
        dmGraphics::SetVertexStream(gfx_context, 1, 2, dmGraphics::TYPE_FLOAT, renderdata->m_VertexStride, (void*)uvs);
        // alpha
        dmGraphics::SetVertexStream(gfx_context, 2, 1, dmGraphics::TYPE_FLOAT, renderdata->m_VertexStride, (void*)alpha);



        dmGraphics::SetVertexProgram(gfx_context, dmGraphics::GetMaterialVertexProgram(renderdata->m_Material));
        dmGraphics::SetVertexConstantBlock(gfx_context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
        dmGraphics::SetFragmentProgram(gfx_context, dmGraphics::GetMaterialFragmentProgram(renderdata->m_Material));

        dmGraphics::SetTexture(gfx_context, renderdata->m_Texture);

        dmGraphics::Draw(gfx_context, dmGraphics::PRIMITIVE_QUADS, renderdata->m_VertexIndex, renderdata->m_VertexCount);

        dmGraphics::SetDepthMask(gfx_context, 1);


    }

}
