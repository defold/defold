#ifndef RENDER_H
#define RENDER_H

#include <string.h>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/hash.h>
#include <script/script.h>
#include <script/lua_source_ddf.h>
#include <graphics/graphics.h>
#include "render/material_ddf.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

    extern const char* RENDER_SOCKET_NAME;

    typedef struct RenderContext*           HRenderContext;
    typedef struct RenderTargetSetup*       HRenderTargetSetup;
    typedef uint64_t                        HRenderType;
    typedef struct NamedConstantBuffer*     HNamedConstantBuffer;
    typedef struct RenderScript*            HRenderScript;
    typedef struct RenderScriptInstance*    HRenderScriptInstance;
    typedef struct Material*                HMaterial;

    /**
     * Font map handle
     */
    typedef struct FontMap* HFontMap;

    /**
     * Display profiles handle
     */
    typedef struct DisplayProfiles* HDisplayProfiles;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_INVALID_CONTEXT = -1,
        RESULT_OUT_OF_RESOURCES = -2,
        RESULT_BUFFER_IS_FULL = -3,
        RESULT_INVALID_PARAMETER = -4,
    };

    enum RenderScriptResult
    {
        RENDER_SCRIPT_RESULT_FAILED = -1,
        RENDER_SCRIPT_RESULT_NO_FUNCTION = 0,
        RENDER_SCRIPT_RESULT_OK = 1
    };

    enum TextAlign
    {
        TEXT_ALIGN_LEFT = 0,
        TEXT_ALIGN_CENTER = 1,
        TEXT_ALIGN_RIGHT = 2
    };

    enum TextVAlign
    {
        TEXT_VALIGN_TOP = 0,
        TEXT_VALIGN_MIDDLE = 1,
        TEXT_VALIGN_BOTTOM = 2
    };

    struct Predicate
    {
        static const uint32_t MAX_TAG_COUNT = 32;
        dmhash_t m_Tags[MAX_TAG_COUNT];
        uint32_t m_TagCount;
    };

    struct Constant
    {
        Vectormath::Aos::Vector4                m_Value;
        dmhash_t                                m_NameHash;
        dmRenderDDF::MaterialDesc::ConstantType m_Type;
        int32_t                                 m_Location;

        Constant() {}
        Constant(dmhash_t name_hash, int32_t location)
            : m_Value(Vectormath::Aos::Vector4(0)), m_NameHash(name_hash), m_Type(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER), m_Location(location)
        {
        }
    };

    struct StencilTestParams
    {
        StencilTestParams();
        void Init();

        dmGraphics::CompareFunc m_Func;
        dmGraphics::StencilOp m_OpSFail;
        dmGraphics::StencilOp m_OpDPFail;
        dmGraphics::StencilOp m_OpDPPass;
        uint32_t m_Ref : 8;
        uint32_t m_RefMask : 8;
        uint32_t m_BufferMask : 8;
        uint8_t m_ColorBufferMask : 4;
        uint8_t m_ClearBuffer : 1;
        uint8_t m_Padding : 3;
    };

    struct MaterialConstant
    {
        Constant m_Constant;
        dmhash_t m_ElementIds[4];
    };

    struct RenderObject
    {
        RenderObject();
        void Init();
        void ClearConstants();

        static const uint32_t MAX_TEXTURE_COUNT = 8;
        static const uint32_t MAX_CONSTANT_COUNT = 4;
        Constant                        m_Constants[MAX_CONSTANT_COUNT];
        Matrix4                         m_WorldTransform;
        Matrix4                         m_TextureTransform;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        HMaterial                       m_Material;
        dmGraphics::HTexture            m_Textures[MAX_TEXTURE_COUNT];
        dmGraphics::PrimitiveType       m_PrimitiveType;
        dmGraphics::Type                m_IndexType;
        dmGraphics::BlendFactor         m_SourceBlendFactor;
        dmGraphics::BlendFactor         m_DestinationBlendFactor;
        StencilTestParams               m_StencilTestParams;
        uint32_t                        m_VertexStart;
        uint32_t                        m_VertexCount;
        uint8_t                         m_VertexConstantMask;
        uint8_t                         m_FragmentConstantMask;
        uint8_t                         m_SetBlendFactors : 1;
        uint8_t                         m_SetStencilTest : 1;
    };

    struct RenderContextParams
    {
        RenderContextParams();

        dmScript::HContext              m_ScriptContext;
        HFontMap                        m_SystemFontMap;
        void*                           m_VertexProgramData;
        void*                           m_FragmentProgramData;
        uint32_t                        m_MaxRenderTypes;
        uint32_t                        m_MaxInstances;
        uint32_t                        m_MaxRenderTargets;
        uint32_t                        m_VertexProgramDataSize;
        uint32_t                        m_FragmentProgramDataSize;
        uint32_t                        m_MaxCharacters;
        uint32_t                        m_CommandBufferSize;
        /// Max debug vertex count
        /// NOTE: This is per debug-type and not the total sum
        uint32_t                        m_MaxDebugVertexCount;
    };

    enum RenderOrder
    {
        RENDER_ORDER_BEFORE_WORLD = 0,
        RENDER_ORDER_WORLD        = 1,
        RENDER_ORDER_AFTER_WORLD  = 2,
    };

    typedef uint8_t HRenderListDispatch;
    static const uint8_t RENDERLIST_INVALID_DISPATCH = 0xff;

    struct RenderListEntry
    {
        Point3 m_WorldPosition;
        uint32_t m_Order;
        uint32_t m_BatchKey;
        uint32_t m_TagMask;
        uintptr_t m_UserData;
        uint32_t m_MinorOrder:4;
        uint32_t m_MajorOrder:2;
        uint32_t m_Dispatch:8;
    };

    enum RenderListOperation
    {
        RENDER_LIST_OPERATION_BEGIN,
        RENDER_LIST_OPERATION_BATCH,
        RENDER_LIST_OPERATION_END
    };

    struct RenderListDispatchParams
    {
        HRenderContext m_Context;
        void* m_UserData;
        RenderListOperation m_Operation;
        RenderListEntry* m_Buf;
        uint32_t* m_Begin;
        uint32_t* m_End;
    };

    typedef void (*RenderListDispatchFn)(RenderListDispatchParams const &params);

    static const HRenderType INVALID_RENDER_TYPE_HANDLE = ~0ULL;

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params);
    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context);

    dmScript::HContext GetScriptContext(HRenderContext render_context);

    void RenderListBegin(HRenderContext render_context);
    HRenderListDispatch RenderListMakeDispatch(HRenderContext render_context, RenderListDispatchFn fn, void *user_data);
    RenderListEntry* RenderListAlloc(HRenderContext render_context, uint32_t entries);
    void RenderListSubmit(HRenderContext render_context, RenderListEntry *begin, RenderListEntry *end);
    void RenderListEnd(HRenderContext render_context);

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map);

    Result RegisterRenderTarget(HRenderContext render_context, dmGraphics::HRenderTarget rendertarget, dmhash_t hash);
    dmGraphics::HRenderTarget GetRenderTarget(HRenderContext render_context, dmhash_t hash);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    const Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
    void SetViewMatrix(HRenderContext render_context, const Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection);

    Result AddToRender(HRenderContext context, RenderObject* ro);
    Result ClearRenderObjects(HRenderContext context);

    // Takes the contents of the render list, sorts by view and inserts all the objects in the
    // render list, unless they already are in place from a previous call.
    Result DrawRenderList(HRenderContext context, Predicate* predicate, HNamedConstantBuffer constant_buffer);

    Result Draw(HRenderContext context, Predicate* predicate, HNamedConstantBuffer constant_buffer);
    Result DrawDebug3d(HRenderContext context);
    Result DrawDebug2d(HRenderContext context);

    void EnableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash, const Vectormath::Aos::Vector4& value);
    void DisableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash);

    /**
     * Render debug square. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the left edge of the square
     * @param y0 y coordinate of the upper edge of the square
     * @param x1 x coordinate of the right edge of the square
     * @param y1 y coordinate of the bottom edge of the square
     * @param color Color
     */
    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color);

    /**
     * Render debug triangle in world space.
     * @param context Render context handle
     * @param vertices Vertices of the triangle, CW winding
     * @param color Color
     */
    void Triangle3d(HRenderContext context, Point3 vertices[3], Vector4 color);

    /**
     * Render debug line. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the start of the line
     * @param y0 y coordinate of the start of the line
     * @param x1 x coordinate of the end of the line
     * @param y1 y coordinate of the end of the line
     * @param color0 Color of the start of the line
     * @param color1 Color of the end of the line
     */
    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color0, Vector4 color1);

    /**
     * Line3D Render debug line
     * @param context Render context handle
     * @param start Start point
     * @param end End point
     * @param color Color
     */
    void Line3D(HRenderContext context, Point3 start, Point3 end, Vector4 start_color, Vector4 end_color);

    HRenderScript   NewRenderScript(HRenderContext render_context, dmLuaDDF::LuaSource *source);

    bool            ReloadRenderScript(HRenderContext render_context, HRenderScript render_script, dmLuaDDF::LuaSource *source);

    void            DeleteRenderScript(HRenderContext render_context, HRenderScript render_script);

    HRenderScriptInstance   NewRenderScriptInstance(HRenderContext render_context, HRenderScript render_script);
    void                    DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance);
    void                    SetRenderScriptInstanceRenderScript(HRenderScriptInstance render_script_instance, HRenderScript render_script);
    void                    AddRenderScriptInstanceMaterial(HRenderScriptInstance render_script_instance, const char* material_name, dmRender::HMaterial material);
    void                    ClearRenderScriptInstanceMaterials(HRenderScriptInstance render_script_instance);
    RenderScriptResult      InitRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      DispatchRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      UpdateRenderScriptInstance(HRenderScriptInstance render_script_instance, float dt);
    void                    OnReloadRenderScriptInstance(HRenderScriptInstance render_script_instance);

    // Material
    HMaterial                       NewMaterial(dmRender::HRenderContext render_context, dmGraphics::HVertexProgram vertex_program, dmGraphics::HFragmentProgram fragment_program);
    void                            DeleteMaterial(dmRender::HRenderContext render_context, HMaterial material);
    void                            ApplyMaterialConstants(dmRender::HRenderContext render_context, HMaterial material, const RenderObject* ro);
    void                            ApplyMaterialSampler(dmRender::HRenderContext render_context, HMaterial material, int unit, dmGraphics::HTexture texture);

    dmGraphics::HProgram            GetMaterialProgram(HMaterial material);
    dmGraphics::HVertexProgram      GetMaterialVertexProgram(HMaterial material);
    dmGraphics::HFragmentProgram    GetMaterialFragmentProgram(HMaterial material);
    void                            SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool                            GetMaterialProgramConstant(HMaterial, dmhash_t name_hash, Constant& out_value);

    /** Retrieve info about a hash related to a program constant
     * The function checks if the hash matches a constant or any element of it.
     * In the former case, the available element ids are returned.
     * In the latter, the index of the element is returned (~0u otherwise).
     * @param material Material to search for constants
     * @param name_hash Hash of the constant name or element id
     * @param out_constant_id Hash of the constant name
     * @param out_element_ids List of 4 element ids, only set if the hash matched a constant
     * @param out_element_index Set if the hash matched an element
     * @return True if a constant or element was found
     */
    bool                            GetMaterialProgramConstantInfo(HMaterial material, dmhash_t name_hash, dmhash_t* out_constant_id, dmhash_t* out_element_ids[4], uint32_t* out_element_index);

    /** Retrieve the value of a constant element
     * @param material Material containing the constant
     * @param name_hash Hash of the material name
     * @param element_index Index of the element
     * @param out_value Out var set to the value
     * @return True if the value was retrieved
     */
    bool                            GetMaterialProgramConstantElement(HMaterial material, dmhash_t name_hash, uint32_t element_index, float& out_value);
    void                            SetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, Vectormath::Aos::Vector4 constant);
    int32_t                         GetMaterialConstantLocation(HMaterial material, dmhash_t name_hash);
    void                            SetMaterialSampler(HMaterial material, dmhash_t name_hash, int16_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter);
    HRenderContext                  GetMaterialRenderContext(HMaterial material);

    uint64_t                        GetMaterialUserData1(HMaterial material);
    void                            SetMaterialUserData1(HMaterial material, uint64_t user_data);
    uint64_t                        GetMaterialUserData2(HMaterial material);
    void                            SetMaterialUserData2(HMaterial material, uint64_t user_data);

    HNamedConstantBuffer            NewNamedConstantBuffer();
    void                            DeleteNamedConstantBuffer(HNamedConstantBuffer buffer);
    void                            SetNamedConstant(HNamedConstantBuffer buffer, const char* name, Vectormath::Aos::Vector4 value);
    bool                            GetNamedConstant(HNamedConstantBuffer buffer, const char* name, Vectormath::Aos::Vector4& value);
    void                            ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer);

    uint32_t                        GetMaterialTagMask(HMaterial material);
    void                            AddMaterialTag(HMaterial material, dmhash_t tag);
    void                            ClearMaterialTags(HMaterial material);
    uint32_t                        ConvertMaterialTagsToMask(dmhash_t* tags, uint32_t tag_count);

}

#endif /* RENDER_H */
