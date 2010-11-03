#ifndef GAMESYS_PRIVATE_H
#define GAMESYS_PRIVATE_H

#include <render/render.h>

namespace dmGameSystem
{
    void RenderTypeParticleBegin(dmRender::HRenderContext rendercontext);
    void RenderTypeParticleDraw(dmRender::HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);
    void RenderTypeParticleEnd(dmRender::HRenderContext rendercontext);

    extern uint32_t g_ModelRenderType;

    void RenderTypeModelBegin(dmRender::HRenderContext rendercontext);
    void RenderTypeModelDraw(dmRender::HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);
    void RenderTypeModelEnd(dmRender::HRenderContext rendercontext);
}
#endif // GAMESYS_PRIVATE_H
