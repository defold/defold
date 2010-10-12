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



    void RenderTypeParticleSetup(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count)
    {
        RenderObject* ro = (RenderObject*)*ro_;

        float* vertex_buffer = (float*)dmRender::GetUserData(ro, 0);
        uint32_t vertex_size = GetUserData(ro, 1);

        dmGraphics::HContext gfx_context = rendercontext->m_GFXContext;

        dmGraphics::SetBlendFunc(gfx_context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(gfx_context, dmGraphics::BLEND);

        dmGraphics::SetDepthMask(gfx_context, 0);

        // positions
        dmGraphics::SetVertexStream(gfx_context, 0, 3, dmGraphics::TYPE_FLOAT, vertex_size, (void*)vertex_buffer);
        // uv's
        dmGraphics::SetVertexStream(gfx_context, 1, 2, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[3]);
        // alpha
        dmGraphics::SetVertexStream(gfx_context, 2, 1, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[5]);
    }

    void RenderTypeParticleDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count)
    {
        RenderObject* ro = (RenderObject*)*ro_;

        dmGraphics::HContext gfx_context = rendercontext->m_GFXContext;


        dmGraphics::HMaterial material = (dmGraphics::HMaterial)GetUserData(ro, 0);
        dmGraphics::HTexture texture = (dmGraphics::HTexture)GetUserData(ro, 1);

        dmGraphics::SetVertexProgram(gfx_context, dmGraphics::GetMaterialVertexProgram(material));
        dmGraphics::SetVertexConstantBlock(gfx_context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
        dmGraphics::SetFragmentProgram(gfx_context, dmGraphics::GetMaterialFragmentProgram(material));

        dmGraphics::SetTexture(gfx_context, texture);

        dmGraphics::Draw(gfx_context, dmGraphics::PRIMITIVE_QUADS, GetUserData(ro, 3), GetUserData(ro, 2));

    }

    void RenderTypeParticleEnd(const RenderContext* rendercontext)
    {
        dmGraphics::SetDepthMask(rendercontext->m_GFXContext, 1);
    }

}
