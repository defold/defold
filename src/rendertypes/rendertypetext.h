#ifndef RENDERTYPETEXT_H_
#define RENDERTYPETEXT_H_

#include "render/render.h"
#include "renderinternal.h"
#include "rendertypedata.h"

namespace dmRender
{
    struct SFontVertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Colour[4];
    };

    void RenderTypeTextSetup(const RenderContext* rendercontext);
    void RenderTypeTextDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count);
}


#endif /* RENDERTYPETEXT_H_ */
