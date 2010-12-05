#ifndef GAMESYS_PRIVATE_H
#define GAMESYS_PRIVATE_H

#include <render/render.h>

namespace dmGameSystem
{
    void RenderTypeParticleBegin(dmRender::HRenderContext rendercontext, void* user_context);
    void RenderTypeParticleDraw(dmRender::HRenderContext rendercontext, void* user_context, dmRender::RenderObject* ro, uint32_t count);
    void RenderTypeParticleEnd(dmRender::HRenderContext rendercontext, void* user_context);

    extern uint32_t g_ModelRenderType;

    void RenderTypeModelBegin(dmRender::HRenderContext rendercontext, void* user_context);
    void RenderTypeModelDraw(dmRender::HRenderContext rendercontext, void* user_context, dmRender::RenderObject* ro, uint32_t count);
    void RenderTypeModelEnd(dmRender::HRenderContext rendercontext, void* user_context);
}
#endif // GAMESYS_PRIVATE_H
