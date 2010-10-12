#ifndef RENDERTYPEPARTICLE_H_
#define RENDERTYPEPARTICLE_H_

#include <graphics/graphics_device.h>
#include <graphics/material.h>
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypedata.h"

namespace dmRender
{
    void RenderTypeParticleEnd(const RenderContext* rendercontext);
    void RenderTypeParticleSetup(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count);
    void RenderTypeParticleDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count);
}


#endif /* RENDERTYPEPARTICLE_H_ */
