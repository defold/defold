#ifndef RENDERTYPEMODEL_H
#define RENDERTYPEMODEL_H

#include "rendercontext.h"
#include "renderinternal.h"

namespace dmRender
{
	void RenderTypeModelSetup(RenderContext* rendercontext);
    void RenderTypeModelDraw(RenderContext* rendercontext, RenderObject* ro);
}



#endif
