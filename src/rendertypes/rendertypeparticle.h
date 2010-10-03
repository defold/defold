#ifndef RENDERTYPEPARTICLE_H_
#define RENDERTYPEPARTICLE_H_

#include <graphics/graphics_device.h>
#include <graphics/material.h>
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypedata.h"

namespace dmRender
{



    void RenderTypeParticleSetup(RenderContext* rendercontext);
    void RenderTypeParticleDraw(RenderContext* rendercontext, RenderObject* ro);
}


#endif /* RENDERTYPEPARTICLE_H_ */
