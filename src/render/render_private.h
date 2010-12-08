#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>

#include "material.h"

#include "render.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

#define DEBUG_3D_NAME "_debug3d"
#define DEBUG_2D_NAME "_debug2d"

    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_FACE,
        DEBUG_RENDER_TYPE_LINE,
        MAX_DEBUG_RENDER_TYPE_COUNT
    };

    struct DebugRenderer
    {
        Predicate                       m_3dPredicate;
        Predicate                       m_2dPredicate;
        dmRender::HRenderContext        m_RenderContext;
        dmRender::RenderObject          m_RenderObject3d[MAX_DEBUG_RENDER_TYPE_COUNT];
        dmRender::RenderObject          m_RenderObject2d[MAX_DEBUG_RENDER_TYPE_COUNT];
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        uint32_t                        m_VertexIndex;
    };

    struct TextContext
    {
        dmArray<dmRender::RenderObject>     m_RenderObjects;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmGraphics::HVertexDeclaration      m_VertexDecl;
        uint32_t                            m_RenderObjectIndex;
        uint32_t                            m_VertexIndex;
        uint32_t                            m_MaxVertexCount;
    };

    struct RenderTargetSetup
    {
        dmGraphics::HRenderTarget   m_RenderTarget;
        uint32_t                    m_Hash;
    };

    struct RenderContext
    {
        dmArray<RenderTargetSetup>  m_RenderTargets;
        dmArray<RenderObject*>      m_RenderObjects;
        DebugRenderer               m_DebugRenderer;
        TextContext                 m_TextContext;

        dmGraphics::HContext        m_GFXContext;

        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;

        uint32_t                    m_DisplayWidth;
        uint32_t                    m_DisplayHeight;

        uint32_t                    m_OutOfResources : 1;
    };

    void RenderTypeTextBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeTextDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro_, uint32_t count);

    void RenderTypeDebugBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeDebugDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro, uint32_t count);
}

#endif

