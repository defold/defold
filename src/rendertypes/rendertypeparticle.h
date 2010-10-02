#ifndef RENDERTYPEPARTICLE_H_
#define RENDERTYPEPARTICLE_H_

#include <graphics/graphics_device.h>
#include <graphics/material.h>
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypedata.h"

namespace dmRender
{

    struct SParticleRenderData
    {
        dmGraphics::HMaterial   m_Material;
        dmGraphics::HTexture    m_Texture;
        float*                  m_VertexData;
        uint32_t                m_VertexStride;
        uint32_t                m_VertexIndex;
        uint32_t                m_VertexCount;
    };


    void RenderTypeParticleSetup(RenderContext* rendercontext);
    void RenderTypeParticleDraw(RenderContext* rendercontext, RenderObject* ro);
}


#endif /* RENDERTYPEPARTICLE_H_ */
