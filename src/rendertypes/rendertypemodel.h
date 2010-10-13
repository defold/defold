#ifndef RENDERTYPEMODEL_H
#define RENDERTYPEMODEL_H

#include "render/render.h"
#include "renderinternal.h"

namespace dmRender
{
	void RenderTypeModelSetup(const RenderContext* rendercontext);
    void RenderTypeModelDraw(const RenderContext* rendercontext, const HRenderObject* ro_, uint32_t count);
    void RenderTypeModelEnd(const RenderContext* rendercontext);
}



#endif
