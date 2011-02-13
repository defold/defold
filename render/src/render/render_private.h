#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>

#include "material.h"

#include "render.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

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
        dmhash_t                    m_Hash;
    };

    struct RenderScriptContext
    {
        RenderScriptContext();

        lua_State*                  m_LuaState;
        dmMessage::HSocket          m_Socket;
        dmMessage::DispatchCallback m_DispatchCallback;
        uint32_t                    m_CommandBufferSize;
    };

    struct RenderContext
    {
        Vectormath::Aos::Vector4    m_VertexConstants[MAX_CONSTANT_COUNT];
        Vectormath::Aos::Vector4    m_FragmentConstants[MAX_CONSTANT_COUNT];
        dmGraphics::HTexture        m_Textures[RenderObject::MAX_TEXTURE_COUNT];
        DebugRenderer               m_DebugRenderer;
        TextContext                 m_TextContext;
        RenderScriptContext         m_RenderScriptContext;
        dmArray<RenderTargetSetup>  m_RenderTargets;
        dmArray<RenderObject*>      m_RenderObjects;

        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;

        dmGraphics::HContext        m_GraphicsContext;

        HMaterial                   m_Material;

        uint32_t                    m_VertexConstantMask;
        uint32_t                    m_FragmentConstantMask;

        uint32_t                    m_OutOfResources : 1;
    };

    void RenderTypeTextBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeTextDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro_, uint32_t count);

    void RenderTypeDebugBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeDebugDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro, uint32_t count);

    Result GenerateKey(HRenderContext render_context, const Matrix4& view_matrix);

}

#endif

