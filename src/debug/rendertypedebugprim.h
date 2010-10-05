#ifndef RENDERTYPEDBEUGPRIM_H_
#define RENDERTYPEDEBUGPRIM_H_

#include "render/render.h"

namespace dmRenderDebug
{

    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_SQUARE,
        DEBUG_RENDER_TYPE_CUBE,
        DEBUG_RENDER_TYPE_PLANE,
        DEBUG_RENDER_TYPE_LINE,
        DEBUG_RENDER_TYPE_LINES
    };

    struct DebugRenderInfo
    {
        DebugRenderType m_Type;
        void*           m_Data0;
        void*           m_Data1;
        void*           m_Data2;
        void*           m_Data3;
        void*           m_Data4;
        void*           m_Data5;
    };

    void RenderTypeDebugPrimSetup(const dmRender::RenderContext* rendercontext);
    void RenderTypeDebugPrimDraw(const dmRender::RenderContext* rendercontext, const dmRender::HRenderObject* ro, uint32_t count);
}


#endif /* RENDERTYPEDEBUGPRIM_H_ */
