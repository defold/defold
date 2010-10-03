#ifndef RENDERTYPEDATA_H_
#define RENDERTYPEDATA_H_

#include <graphics/material.h>

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

}
#endif /* RENDERTYPEDATA_H_ */
