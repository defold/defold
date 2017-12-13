#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/message.h>
#include <dlib/hashtable.h>

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

    struct Material
    {

        Material()
        : m_RenderContext(0)
        , m_Program(0)
        , m_VertexProgram(0)
        , m_FragmentProgram(0)
        , m_TagMask(0)
        , m_UserData1(0)
        , m_UserData2(0)
        {
        }

        dmRender::HRenderContext                m_RenderContext;
        dmGraphics::HProgram                    m_Program;
        dmGraphics::HVertexProgram              m_VertexProgram;
        dmGraphics::HFragmentProgram            m_FragmentProgram;
        dmHashTable64<int32_t>                  m_NameHashToLocation;
        dmArray<MaterialConstant>               m_Constants;
        dmArray<Sampler>                        m_Samplers;
        uint32_t                                m_TagMask;
        uint64_t                                m_UserData1;
        uint64_t                                m_UserData2;
    };

    // The order of this enum also defines the order in which the corresponding ROs should be rendered
    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_FACE_3D,
        DEBUG_RENDER_TYPE_LINE_3D,
        DEBUG_RENDER_TYPE_FACE_2D,
        DEBUG_RENDER_TYPE_LINE_2D,
        MAX_DEBUG_RENDER_TYPE_COUNT
    };

    struct DebugRenderTypeData
    {
        dmRender::RenderObject  m_RenderObject;
        void*                   m_ClientBuffer;
    };

    struct DebugRenderer
    {
        DebugRenderTypeData             m_TypeData[MAX_DEBUG_RENDER_TYPE_COUNT];
        Predicate                       m_3dPredicate;
        Predicate                       m_2dPredicate;
        dmRender::HRenderContext        m_RenderContext;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        uint32_t                        m_MaxVertexCount;
        uint32_t                        m_RenderBatchVersion;
    };

    const int MAX_TEXT_RENDER_CONSTANTS = 4;

    struct TextEntry
    {
        StencilTestParams   m_StencilTestParams;
        Matrix4             m_Transform;
        dmRender::Constant  m_RenderConstants[MAX_TEXT_RENDER_CONSTANTS];
        HFontMap            m_FontMap;
        HMaterial           m_Material;
        dmGraphics::BlendFactor m_SourceBlendFactor;
        dmGraphics::BlendFactor m_DestinationBlendFactor;
        uint64_t            m_BatchKey;
        uint32_t            m_FaceColor;
        uint32_t            m_StringOffset;
        uint32_t            m_OutlineColor;
        uint32_t            m_ShadowColor;
        uint16_t            m_RenderOrder;
        uint8_t             m_NumRenderConstants;
        bool                m_LineBreak;
        float               m_Width;
        float               m_Height;
        float               m_Leading;
        float               m_Tracking;
        int32_t             m_Next;
        int32_t             m_Tail;
        uint32_t            m_Align : 2;
        uint32_t            m_VAlign : 2;
        uint32_t            m_StencilTestParamsSet : 1;
    };

    struct TextContext
    {
        dmArray<dmRender::RenderObject>     m_RenderObjects;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        void*                               m_ClientBuffer;
        dmGraphics::HVertexDeclaration      m_VertexDecl;
        uint32_t                            m_RenderObjectIndex;
        uint32_t                            m_VertexIndex;
        uint32_t                            m_MaxVertexCount;
        uint32_t                            m_VerticesFlushed;
        dmArray<char>                       m_TextBuffer;
        // Map from batch id (hash of font-map etc) to index into m_TextEntries
        dmArray<TextEntry>                  m_TextEntries;
        uint32_t                            m_TextEntriesFlushed;
        uint32_t                            m_Frame;
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
        uint32_t                    m_CommandBufferSize;
    };

    struct RenderListDispatch
    {
        RenderListDispatchFn m_Fn;
        void *m_UserData;
    };

    struct RenderListSortValue
    {
        union
        {
            struct
            {
                uint32_t m_BatchKey:24;
                uint32_t m_Dispatch:8;
                uint32_t m_Order:24;
                uint32_t m_MajorOrder:8;
            };
            // only temporarily used
            float m_ZW;
            // final sort value
            uint64_t m_SortKey;
        };
    };

    struct RenderContext
    {
        dmGraphics::HTexture        m_Textures[RenderObject::MAX_TEXTURE_COUNT];
        dmGraphics::HRenderTarget   m_CurrentRenderTarget;
        DebugRenderer               m_DebugRenderer;
        TextContext                 m_TextContext;
        dmResource::HFactory        m_ResourceFactory;
        dmScript::HContext          m_ScriptContext;
        RenderScriptContext         m_RenderScriptContext;
        dmArray<RenderTargetSetup>  m_RenderTargets;
        dmArray<RenderObject*>      m_RenderObjects;

        dmArray<RenderListEntry>    m_RenderList;
        dmArray<RenderListDispatch> m_RenderListDispatch;
        dmArray<RenderListSortValue>m_RenderListSortValues;
        dmArray<uint32_t>           m_RenderListSortBuffer;
        dmArray<uint32_t>           m_RenderListSortIndices;

        HFontMap                    m_SystemFontMap;

        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;

        dmGraphics::HContext        m_GraphicsContext;

        HMaterial                   m_Material;

        dmMessage::HSocket          m_Socket;

        uint32_t                    m_OutOfResources : 1;
        uint32_t                    m_StencilBufferCleared : 1;
    };

    void RenderTypeTextBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeTextDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro_, uint32_t count);

    void RenderTypeDebugBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeDebugDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro, uint32_t count);

    Result GenerateKey(HRenderContext render_context, const Matrix4& view_matrix);

    void ApplyRenderObjectConstants(HRenderContext render_context, HMaterial material, const struct RenderObject* ro);

}

#endif

