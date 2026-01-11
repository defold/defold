// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <string.h> // For memset

#include <dmsdk/dlib/vmath.h>
#include <dlib/opaque_handle_container.h>

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
    using namespace dmVMath;

    struct FontRenderBackend;
    typedef FontRenderBackend* HFontRenderBackend;

#define DEBUG_3D_NAME "_debug3d"
#define DEBUG_2D_NAME "_debug2d"

    struct Sampler
    {
        dmhash_t                  m_NameHash;
        dmGraphics::TextureType   m_Type;
        dmGraphics::TextureFilter m_MinFilter;
        dmGraphics::TextureFilter m_MagFilter;
        dmGraphics::TextureWrap   m_UWrap;
        dmGraphics::TextureWrap   m_VWrap;
        dmGraphics::HUniformLocation m_Location;
        float                     m_MaxAnisotropy;
        uint8_t                   m_UnitValueCount;

        Sampler()
            : m_NameHash(0)
            , m_Type(dmGraphics::TEXTURE_TYPE_2D)
            , m_MinFilter(dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
            , m_MagFilter(dmGraphics::TEXTURE_FILTER_LINEAR)
            , m_UWrap(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE)
            , m_VWrap(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE)
            , m_Location(dmGraphics::INVALID_UNIFORM_LOCATION)
            , m_MaxAnisotropy(1.0f)
            , m_UnitValueCount(0)
        {
        }
    };

    struct MaterialAttribute
    {
        dmhash_t m_ElementIds[4];
        int32_t  m_Location;
        uint16_t m_ValueIndex;
        uint16_t m_ValueCount;
    };

    struct Material
    {
        Material()
        {
            memset(this, 0, sizeof(*this));
            m_VertexSpace = dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL;
        }

        dmRender::HRenderContext                m_RenderContext;
        dmGraphics::HProgram                    m_Program;
        dmGraphics::HVertexDeclaration          m_VertexDeclarationShared;
        dmGraphics::HVertexDeclaration          m_VertexDeclarationPerVertex;
        dmGraphics::HVertexDeclaration          m_VertexDeclarationPerInstance;
        dmGraphics::VertexAttributeInfoMetadata m_VertexAttributeInfoMetadata;
        dmHashTable64<dmGraphics::HUniformLocation> m_NameHashToLocation;
        dmArray<dmGraphics::VertexAttribute>    m_VertexAttributes;
        dmArray<MaterialAttribute>              m_MaterialAttributes;
        dmArray<uint8_t>                        m_MaterialAttributeValues;
        dmArray<RenderConstant>                 m_Constants;
        dmArray<Sampler>                        m_Samplers;
        dmRenderDDF::MaterialDesc::PbrParameters m_PbrParameters;
        uint32_t                                m_TagListKey; // the key to use with GetMaterialTagList()
        dmRenderDDF::MaterialDesc::VertexSpace  m_VertexSpace;
        uint8_t                                 m_InstancingSupported : 1;
        uint8_t                                 m_HasSkinnedAttributes : 1;
        uint8_t                                 m_HasSkinnedMatrixCache : 1;
    };

    struct ComputeProgram
    {
        dmRender::HRenderContext                    m_RenderContext;
        dmGraphics::HProgram                        m_Program;
        dmArray<RenderConstant>                     m_Constants;
        dmArray<Sampler>                            m_Samplers;
        dmHashTable64<dmGraphics::HUniformLocation> m_NameHashToLocation;
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

    const int MAX_TEXT_RENDER_CONSTANTS = 16;

    struct TextEntry
    {
        StencilTestParams   m_StencilTestParams;
        Matrix4             m_Transform;
        HConstant           m_RenderConstants[MAX_TEXT_RENDER_CONSTANTS];
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
        dmVMath::Point3     m_FrustumCullingCenter;
        float               m_FrustumCullingRadiusSq;
        uint32_t            m_Align : 2;
        uint32_t            m_VAlign : 2;
        uint32_t            m_StencilTestParamsSet : 1;
    };

    struct TextContext
    {
        dmArray<dmRender::RenderObject>         m_RenderObjects;
        dmArray<dmRender::HNamedConstantBuffer> m_ConstantBuffers;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        void*                               m_ClientBuffer;
        dmGraphics::HVertexDeclaration      m_VertexDecl;
        HFontRenderBackend                  m_FontRenderBackend;
        uint32_t                            m_RenderObjectIndex;
        uint32_t                            m_VertexIndex;
        uint32_t                            m_MaxVertexCount;
        uint32_t                            m_VerticesFlushed;
        dmArray<char>                       m_TextBuffer;
        // Map from batch id (hash of font-map etc) to index into m_TextEntries
        dmArray<TextEntry>                  m_TextEntries;
        uint32_t                            m_TextEntriesFlushed;
        uint32_t                            m_Frame;
        uint32_t                            m_PreviousFrame;
    };

    struct RenderScriptContext
    {
        RenderScriptContext();

        lua_State*                  m_LuaState;
        uint32_t                    m_CommandBufferSize;
    };

    struct RenderListDispatch
    {
        RenderListDispatchFn        m_DispatchFn;
        RenderListVisibilityFn      m_VisibilityFn;
        void*                       m_UserData;
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
                uint32_t m_MajorOrder:4;        // currently only 2 bits used (dmRender::RenderOrder)
                uint32_t m_MinorOrder:4;
            };
            // only temporarily used
            float m_ZW;
            // final sort value
            uint64_t m_SortKey;
        };
    };

    struct RenderListRange
    {
        uint32_t m_TagListKey;
        uint32_t m_Start;       // Index into the renderlist
        uint32_t m_Count:31;
        uint32_t m_Skip:1;      // During the current draw call
    };

    struct MaterialTagList
    {
        uint32_t m_Count;
        dmhash_t m_Tags[MAX_MATERIAL_TAG_COUNT];
    };

    struct TextureBinding
    {
        dmhash_t             m_Samplerhash;
        dmGraphics::HTexture m_Texture;
    };

    struct RenderCamera
    {
        dmMessage::URL   m_URL;
        HOpaqueHandle    m_Handle;
        dmVMath::Matrix4 m_View;
        dmVMath::Matrix4 m_Projection;
        dmVMath::Matrix4 m_ViewProjection;

        // These are cached each update in case
        // the camera data has changed and we need to update
        // based on the new parameters
        dmVMath::Point3  m_LastPosition;
        dmVMath::Quat    m_LastRotation;

        RenderCameraData m_Data;
        uint8_t          m_UseFrustum : 1;
        uint8_t          m_Dirty      : 1;
        uint8_t          m_Enabled    : 1;
    };

    struct RenderContext
    {
        DebugRenderer               m_DebugRenderer;
        TextContext                 m_TextContext;
        dmScript::HContext          m_ScriptContext;
        RenderScriptContext         m_RenderScriptContext;
        dmArray<RenderObject*>      m_RenderObjects;
        dmScript::ScriptWorld*      m_ScriptWorld;
        dmScript::LuaCallbackInfo*  m_CallbackInfo;

        dmArray<RenderListEntry>    m_RenderList;
        dmArray<RenderListDispatch> m_RenderListDispatch;
        dmArray<RenderListSortValue>m_RenderListSortValues;
        dmArray<uint32_t>           m_RenderListSortBuffer;
        dmArray<uint32_t>           m_RenderListSortIndices;
        dmArray<RenderListRange>    m_RenderListRanges;         // Maps tagmask to a range in the (sorted) render list
        dmArray<TextureBinding>     m_TextureBindTable;
        dmhash_t                    m_FrustumHash;

        dmHashTable32<MaterialTagList>  m_MaterialTagLists;

        dmOpaqueHandleContainer<RenderCamera> m_RenderCameras;
        HRenderCamera                         m_CurrentRenderCamera; // When != 0, the renderer will use the matrices from this camera.

        HFontMap                    m_SystemFontMap;
        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;
        dmGraphics::HContext        m_GraphicsContext;
        HMaterial                   m_Material;
        HComputeProgram             m_ComputeProgram;
        dmMessage::HSocket          m_Socket;
        uint32_t                    m_OutOfResources                : 1;
        uint32_t                    m_StencilBufferCleared          : 1;
        uint32_t                    m_MultiBufferingRequired        : 1;
        uint32_t                    m_CurrentRenderCameraUseFrustum : 1;
        uint32_t                    m_IsRenderPaused                : 1;
    };

    struct BufferedRenderBuffer
    {
        dmArray<HRenderBuffer> m_Buffers;
        RenderBufferType       m_Type;
        uint16_t               m_BufferIndex;
    };

    void RenderTypeTextBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeTextDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro_, uint32_t count);

    void RenderTypeDebugBegin(HRenderContext rendercontext, void* user_context);
    void RenderTypeDebugDraw(HRenderContext rendercontext, void* user_context, RenderObject* ro, uint32_t count);

    Result GenerateKey(HRenderContext render_context, const Matrix4& view_matrix);

    void     GetProgramUniformCount(dmGraphics::HProgram program, uint32_t total_constants_count, uint32_t* constant_count_out, uint32_t* samplers_count_out);
    void     SetProgramConstantValues(dmGraphics::HContext graphics_context, dmGraphics::HProgram program, uint32_t total_constants_count, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmArray<RenderConstant>& constants, dmArray<Sampler>& samplers);
    void     SetProgramConstant(dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, const dmVMath::Matrix4& world_matrix, const dmVMath::Matrix4& texture_matrix, dmGraphics::ShaderDesc::Language program_language, dmRenderDDF::MaterialDesc::ConstantType type, dmGraphics::HProgram program, dmGraphics::HUniformLocation location, HConstant constant);
    void     SetProgramRenderConstant(const dmArray<RenderConstant>& constants, dmhash_t name_hash, const dmVMath::Vector4* values, uint32_t count);
    void     SetProgramConstantType(const dmArray<RenderConstant>& constants, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool     GetProgramConstant(const dmArray<RenderConstant>& constants, dmhash_t name_hash, HConstant& out_value);
    bool     SetProgramSampler(dmArray<Sampler>& samplers, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);
    uint32_t GetProgramSamplerUnit(const dmArray<Sampler>& samplers, dmhash_t name_hash);
    int32_t  GetProgramSamplerIndex(const dmArray<Sampler>& samplers, dmhash_t name_hash);
    HSampler GetProgramSampler(const dmArray<Sampler>& samplers, uint32_t unit);
    void     ApplyProgramSampler(dmRender::HRenderContext render_context, HSampler sampler, uint8_t unit, dmGraphics::HTexture texture);

    void FillElementIds(const char* name, char* buffer, uint32_t buffer_size, dmhash_t element_ids[4]);

    // Return true if the predicate tags all exist in the material tag list
    bool                            MatchMaterialTags(uint32_t material_tag_count, const dmhash_t* material_tags, uint32_t tag_count, const dmhash_t* tags);
    // Returns a hashkey that the material can use to get the list
    uint32_t                        RegisterMaterialTagList(HRenderContext context, uint32_t tag_count, const dmhash_t* tags);
    // Gets the list associated with a hash of all the tags (see RegisterMaterialTagList)
    void                            GetMaterialTagList(HRenderContext context, uint32_t list_hash, MaterialTagList* list);

    void    SetTextureBindingByHash(dmRender::HRenderContext render_context, dmhash_t sampler_hash, dmGraphics::HTexture texture);
    void    SetTextureBindingByUnit(dmRender::HRenderContext render_context, uint32_t unit, dmGraphics::HTexture texture);
    bool    GetCanBindTexture(dmGraphics::HContext context, dmGraphics::HTexture texture, HSampler sampler, uint32_t unit);
    int32_t GetMaterialSamplerIndex(HMaterial material, dmhash_t name_hash);

    void    DispatchCompute(HRenderContext render_context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z, HNamedConstantBuffer constant_buffer);
    void    ApplyComputeProgramConstants(HRenderContext render_context, HComputeProgram compute_program);
    int32_t GetComputeProgramSamplerIndex(HComputeProgram program, dmhash_t name_hash);

    // Render camera
    RenderCamera* GetRenderCameraByUrl(HRenderContext render_context, const dmMessage::URL& camera_url);
    RenderCamera* CheckRenderCamera(lua_State* L, int index, HRenderContext render_context);

    // Exposed here for unit testing
    struct RenderListEntrySorter
    {
        bool operator()(int a, int b) const
        {
            const RenderListEntry& ea = m_Base[a];
            const RenderListEntry& eb = m_Base[b];
            return ea.m_TagListKey < eb.m_TagListKey;
        }
        RenderListEntry* m_Base;
    };

    struct FindRangeComparator
    {
        RenderListEntry* m_Entries;
        bool operator() (const uint32_t& a, const uint32_t& b) const
        {
            return m_Entries[a].m_TagListKey < m_Entries[b].m_TagListKey;
        }
    };

    typedef void (*RangeCallback)(void* ctx, uint32_t val, size_t start, size_t count);
    typedef void (*ContextEventCallback)(void* ctx, RenderContextEvent event_type);

    // Invokes the callback for each range. Two ranges are not guaranteed to preceed/succeed one another.
    void FindRenderListRanges(uint32_t* first, size_t offset, size_t size, RenderListEntry* entries, FindRangeComparator& comp, void* ctx, RangeCallback callback );

    bool FindTagListRange(RenderListRange* ranges, uint32_t num_ranges, uint32_t tag_list_key, RenderListRange& range);

    /*
    * Possible event_type see in dmRender::RenderContextEvent enum.
    * Values matches "context_lost"/"context_restored" events from JS code.
    */
    void OnContextEvent(void* context, RenderContextEvent event_type);
    void SetupContextEventCallback(void* context, ContextEventCallback callback);
    void PlatformSetupContextEventCallback(void* context, ContextEventCallback callback);

    // ******************************************************************************************************

    template <typename T>
    class BatchIterator
    {
    public:
        BatchIterator(uint32_t num_entries, T entries, bool (*test_eq_fn)(T prev, T current));
        bool     Next();
        T        Begin();
        uint32_t Length();

    protected:
        T         m_EntriesEnd;
        T         m_Iterator;
        T         m_BatchStart;
        bool      (*m_TestEqFn)(T prev, T current);
    };

    template<typename T>
    BatchIterator<T>::BatchIterator(uint32_t num_entries, T entries, bool (*test_eq_fn)(T prev, T current))
    : m_EntriesEnd(entries + num_entries)
    , m_Iterator(entries)
    , m_BatchStart(0)
    , m_TestEqFn(test_eq_fn)
    {}

    template<typename T>
    bool BatchIterator<T>::Next()
    {
        m_BatchStart = m_Iterator;

        T prev = m_Iterator;
        while (m_Iterator < m_EntriesEnd)
        {
            T next = ++m_Iterator;
            if ((next >= m_EntriesEnd) || !m_TestEqFn(prev, next))
            {
                break;
            }
            prev = next;
        }

        return m_BatchStart < m_EntriesEnd;
    }

    template<typename T>
    T BatchIterator<T>::Begin()
    {
        return m_BatchStart;
    }

    template<typename T>
    uint32_t BatchIterator<T>::Length()
    {
        return (uint32_t)(uintptr_t)(m_Iterator - m_BatchStart);
    }

    // ******************************************************************************************************
}

#endif

