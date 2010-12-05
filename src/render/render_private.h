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

    struct RenderObject
    {
        Vector4                 m_VertexConstants[MAX_CONSTANT_COUNT];
        Vector4                 m_FragmentConstants[MAX_CONSTANT_COUNT];
        Matrix4                 m_WorldTransform;
        Matrix4                 m_TextureTransform;
        HMaterial               m_Material;
        dmGraphics::HTexture    m_Texture;
        void*                   m_UserData;
        uint32_t                m_Type;
        uint32_t                m_VertexStart;
        uint32_t                m_VertexCount;
        uint8_t                 m_VertexConstantMask;
        uint8_t                 m_FragmentConstantMask;
    };

    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_FACE,
        DEBUG_RENDER_TYPE_LINE,
        MAX_DEBUG_RENDER_TYPE_COUNT
    };

    struct DebugRenderer
    {
        Predicate                   m_3dPredicate;
        Predicate                   m_2dPredicate;
        dmRender::HRenderContext    m_RenderContext;
        dmRender::HRenderObject     m_RenderObject3d[MAX_DEBUG_RENDER_TYPE_COUNT];
        dmRender::HRenderObject     m_RenderObject2d[MAX_DEBUG_RENDER_TYPE_COUNT];
    };

    struct TextVertex
    {
        float m_Position[2];
        float m_UV[2];
    };

    struct TextContext
    {
        dmArray<TextVertex>                 m_Vertices;
        dmArray<dmRender::HRenderObject>    m_RenderObjects;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmGraphics::HVertexDeclaration      m_VertexDecl;
        uint32_t                            m_RenderObjectIndex;
        uint32_t                            m_TextRenderType;
    };

    struct RenderTargetSetup
    {
        dmGraphics::HRenderTarget   m_RenderTarget;
        uint32_t                    m_Hash;
    };

    struct RenderContext
    {
        dmArray<RenderTargetSetup>  m_RenderTargets;
        dmArray<RenderType>         m_RenderTypes;
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
    void RenderTypeTextDraw(HRenderContext rendercontext, void* user_context, HRenderObject ro_, uint32_t count);

    void RenderTypeDebugBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeDebugDraw(HRenderContext rendercontext, void* user_context, dmRender::HRenderObject ro, uint32_t count);
}

#endif

