// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_GRAPHICS_H
#define DM_GRAPHICS_H

#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>

#include <dlib/hash.h>
#include <dlib/job_thread.h>
#include <dlib/opaque_handle_container.h>

#include <ddf/ddf.h>
#include <graphics/graphics_ddf.h>

#include <platform/platform_window.h>

namespace dmGraphics
{
    /**
     * Callback function called when the window is requested to close.
     * @param user_data user data that was supplied when opening the window
     * @return whether the window should be closed or not
     */
    typedef bool (*WindowCloseCallback)(void* user_data);

    /**
     * Function used when in the application loop when performing a single iteration.
     * @param user_data user data that will be passed into the method when running the application loop.
     */
    typedef void (*WindowStepMethod)(void* user_data);

    /**
     * Function used to determine whether the application loop should continue running.
     * @param user_data user data used that will be passed into the method when making a determination.
     * @return a non-zero value will indicate that the application should continue running, whereas zero will
     *  lead to the application loop terminating.
     */
    typedef int32_t (*WindowIsRunning)(void* user_data);

    // See documentation in engine.h
    typedef void (*EngineInit)(void* ctx);
    typedef void (*EngineExit)(void* ctx);
    typedef void* (*EngineCreate)(int argc, char** argv);
    typedef void (*EngineDestroy)(void* engine);
    typedef int (*EngineUpdate)(void* engine);
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    // Decorated asset handle with 21 bits meta | 32 bits opaque handle
    // Note: that we can only use a total of 53 bits out of the 64 due to how we expose the handles
    //       to the users via lua: http://lua-users.org/wiki/NumbersTutorial
    typedef uint64_t HAssetHandle;

    const static uint64_t MAX_ASSET_HANDLE_VALUE  = 0x20000000000000-1; // 2^53 - 1
    static const uint8_t  MAX_BUFFER_TYPE_COUNT   = 2 + MAX_BUFFER_COLOR_ATTACHMENTS;
    const static uint8_t  MAX_VERTEX_STREAM_COUNT = 8;

    const static uint8_t DM_GRAPHICS_STATE_WRITE_R = 0x1;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_G = 0x2;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_B = 0x4;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_A = 0x8;

    static const HVertexProgram   INVALID_VERTEX_PROGRAM_HANDLE   = ~0u;
    static const HFragmentProgram INVALID_FRAGMENT_PROGRAM_HANDLE = ~0u;
    static const HUniformLocation INVALID_UNIFORM_LOCATION        = ~0ull;

    enum AdapterFamily
    {
        ADAPTER_FAMILY_NONE   = -1,
        ADAPTER_FAMILY_NULL   = 1,
        ADAPTER_FAMILY_OPENGL = 2,
        ADAPTER_FAMILY_VULKAN = 3,
        ADAPTER_FAMILY_VENDOR = 4,
    };

    enum AssetType
    {
        ASSET_TYPE_NONE          = 0,
        ASSET_TYPE_TEXTURE       = 1,
        ASSET_TYPE_RENDER_TARGET = 2,
    };

    // buffer clear types, each value is guaranteed to be separate bits
    enum BufferType
    {
        BUFFER_TYPE_COLOR0_BIT  = 0x01,
        BUFFER_TYPE_COLOR1_BIT  = 0x02,
        BUFFER_TYPE_COLOR2_BIT  = 0x04,
        BUFFER_TYPE_COLOR3_BIT  = 0x08,
        BUFFER_TYPE_DEPTH_BIT   = 0x10,
        BUFFER_TYPE_STENCIL_BIT = 0x20,
    };

    // render states
    enum State
    {
        STATE_DEPTH_TEST           = 0,
        STATE_SCISSOR_TEST         = 1,
        STATE_STENCIL_TEST         = 2,
        STATE_ALPHA_TEST           = 3,
        STATE_BLEND                = 4,
        STATE_CULL_FACE            = 5,
        STATE_POLYGON_OFFSET_FILL  = 6,
        STATE_ALPHA_TEST_SUPPORTED = 7,
    };

    // Texture type
    enum TextureType
    {
        TEXTURE_TYPE_2D               = 0,
        TEXTURE_TYPE_2D_ARRAY         = 1,
        TEXTURE_TYPE_CUBE_MAP         = 2,
        TEXTURE_TYPE_IMAGE_2D         = 3,
        TEXTURE_TYPE_SAMPLER          = 4,
        TEXTURE_TYPE_TEXTURE_2D       = 5,
        TEXTURE_TYPE_TEXTURE_2D_ARRAY = 6,
        TEXTURE_TYPE_TEXTURE_CUBE     = 7
    };

    // Texture filter
    enum TextureFilter
    {
        TEXTURE_FILTER_DEFAULT                = 0,
        TEXTURE_FILTER_NEAREST                = 1,
        TEXTURE_FILTER_LINEAR                 = 2,
        TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 3,
        TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 4,
        TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 5,
        TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 6,
    };

    // Texture wrap
    enum TextureWrap
    {
        TEXTURE_WRAP_CLAMP_TO_BORDER = 0,
        TEXTURE_WRAP_CLAMP_TO_EDGE   = 1,
        TEXTURE_WRAP_MIRRORED_REPEAT = 2,
        TEXTURE_WRAP_REPEAT          = 3,
    };

    // Face type
    enum FaceType
    {
        FACE_TYPE_FRONT          = 0,
        FACE_TYPE_BACK           = 1,
        FACE_TYPE_FRONT_AND_BACK = 2,
    };

    enum TextureStatusFlags
    {
        TEXTURE_STATUS_OK               = 0,
        TEXTURE_STATUS_DATA_PENDING     = (1 << 0), // Currently waiting for the upload to be done
    };

    enum ContextFeature
    {
        CONTEXT_FEATURE_MULTI_TARGET_RENDERING = 0,
        CONTEXT_FEATURE_TEXTURE_ARRAY          = 1,
        CONTEXT_FEATURE_COMPUTE_SHADER         = 2,
        CONTEXT_FEATURE_STORAGE_BUFFER         = 3,
        CONTEXT_FEATURE_VSYNC                  = 4,
    };

    // Translation table to translate RenderTargetAttachment to BufferType
    struct AttachmentToBufferType
    {
        BufferType m_AttachmentToBufferType[MAX_ATTACHMENT_COUNT];
        AttachmentToBufferType();
    };

    struct TextureCreationParams
    {
        TextureCreationParams()
        : m_Type(TEXTURE_TYPE_2D)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(1)
        , m_OriginalWidth(0)
        , m_OriginalHeight(0)
        , m_MipMapCount(1)
        , m_UsageHintBits(TEXTURE_USAGE_FLAG_SAMPLE)
        {}

        TextureType m_Type;
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint16_t    m_Depth;
        uint16_t    m_OriginalWidth;
        uint16_t    m_OriginalHeight;
        uint8_t     m_MipMapCount;
        uint8_t     m_UsageHintBits;
    };

    struct TextureParams
    {
        TextureParams()
        : m_Data(0x0)
        , m_DataSize(0)
        , m_Format(TEXTURE_FORMAT_RGBA)
        , m_MinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        , m_MagFilter(TEXTURE_FILTER_LINEAR)
        , m_UWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_VWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_X(0)
        , m_Y(0)
        , m_Z(0)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(0)
        , m_MipMap(0)
        , m_SubUpdate(false)
        {}

        const void*   m_Data;
        uint32_t      m_DataSize;
        TextureFormat m_Format;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_UWrap;
        TextureWrap   m_VWrap;

        // For sub texture updates
        uint32_t m_X;
        uint32_t m_Y;
        uint32_t m_Z; // For array texture, this is the slice to set

        uint16_t m_Width;
        uint16_t m_Height;
        uint16_t m_Depth; // For array texture, this is slice count
        uint8_t  m_MipMap    : 7;
        uint8_t  m_SubUpdate : 1;
    };

    struct RenderTargetCreationParams
    {
        TextureCreationParams m_ColorBufferCreationParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureCreationParams m_DepthBufferCreationParams;
        TextureCreationParams m_StencilBufferCreationParams;
        TextureParams         m_ColorBufferParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams         m_DepthBufferParams;
        TextureParams         m_StencilBufferParams;
        AttachmentOp          m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp          m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float                 m_ColorBufferClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];
        // TODO: Depth/Stencil

        uint8_t               m_DepthTexture   : 1;
        uint8_t               m_StencilTexture : 1;
    };

    // Parameters structure for NewContext
    struct ContextParams
    {
        ContextParams();

        dmPlatform::HWindow   m_Window;
        dmJobThread::HContext m_JobThread;
        TextureFilter         m_DefaultTextureMinFilter;
        TextureFilter         m_DefaultTextureMagFilter;
        uint32_t              m_Width;
        uint32_t              m_Height;
        uint32_t              m_GraphicsMemorySize;             // The max allowed Gfx memory (default 0)
        uint32_t              m_SwapInterval;                   // Initial VSync setting (default 1)
        uint8_t               m_VerifyGraphicsCalls : 1;
        uint8_t               m_PrintDeviceInfo : 1;
        uint8_t               m_RenderDocSupport : 1;           // Vulkan only
        uint8_t               m_UseValidationLayers : 1;        // Vulkan only
        uint8_t               : 4;
    };

    struct PipelineState
    {
        uint64_t m_WriteColorMask           : 4;
        uint64_t m_WriteDepth               : 1;
        uint64_t m_PrimtiveType             : 3;
        // Depth Test
        uint64_t m_DepthTestEnabled         : 1;
        uint64_t m_DepthTestFunc            : 3;
        // Stencil Test
        uint64_t m_StencilEnabled           : 1;

        // Front
        uint64_t m_StencilFrontOpFail       : 3;
        uint64_t m_StencilFrontOpPass       : 3;
        uint64_t m_StencilFrontOpDepthFail  : 3;
        uint64_t m_StencilFrontTestFunc     : 3;

        // Back
        uint64_t m_StencilBackOpFail        : 3;
        uint64_t m_StencilBackOpPass        : 3;
        uint64_t m_StencilBackOpDepthFail   : 3;
        uint64_t m_StencilBackTestFunc      : 3;

        uint64_t m_StencilWriteMask         : 8;
        uint64_t m_StencilCompareMask       : 8;
        uint64_t m_StencilReference         : 8;
        // Blending
        uint64_t m_BlendEnabled             : 1;
        uint64_t m_BlendSrcFactor           : 4;
        uint64_t m_BlendDstFactor           : 4;
        // Culling
        uint64_t m_CullFaceEnabled          : 1;
        uint64_t m_CullFaceType             : 2;
        // Face winding
        uint64_t m_FaceWinding              : 1;
        // Polygon offset
        uint64_t m_PolygonOffsetFillEnabled : 1;
    };

    struct VertexAttributeInfo
    {
        dmhash_t                      m_NameHash;
        VertexAttribute::SemanticType m_SemanticType;
        VertexAttribute::DataType     m_DataType;
        CoordinateSpace               m_CoordinateSpace;
        const uint8_t*                m_ValuePtr;
        uint32_t                      m_ValueByteSize;
        uint32_t                      m_ElementCount;
        bool                          m_Normalize;
    };

    struct VertexAttributeInfos
    {
        VertexAttributeInfos()
        {
            memset(this, 0, sizeof(*this));
            m_StructSize = sizeof(*this);
        }

        VertexAttributeInfo m_Infos[MAX_VERTEX_STREAM_COUNT];
        uint32_t            m_VertexStride;
        uint32_t            m_NumInfos;
        uint32_t            m_StructSize;
    };

    /** Creates a graphics context
     * Currently, there can only be one context active at a time.
     * @return New graphics context
     */
    HContext NewContext(const ContextParams& params);

    /**
     * Destroy device
     */
    void DeleteContext(HContext context);

    /**
     * Install a graphics adapter
     * @params family AdapterFamily identifier for which adapter to use (vulkan/opengl/null/vendor)
     * @return True if a graphics backend could be created, false otherwise.
     */
    bool InstallAdapter(AdapterFamily family = ADAPTER_FAMILY_NONE);
    AdapterFamily GetAdapterFamily(const char* adapter_name);
    AdapterFamily GetInstalledAdapterFamily();

    /**
     * Finalize graphics system
     */
    void Finalize();

    /**
     * Starts the app that needs to control the update loop (iOS only)
     */
    void AppBootstrap(int argc, char** argv, void* init_ctx, EngineInit init_fn, EngineExit exit_fn, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);

    /**
     * Get the window refresh rate
     * @params context Graphics context handle
     * @return The window refresh rate, 0 if refresh rate could not be read.
     */
    uint32_t GetWindowRefreshRate(HContext context);

    /**
     * Get the window handle from the graphics context
     * @param context Graphics context handle
     * @return The window handle
     */
    dmPlatform::HWindow GetWindow(HContext context);

    /**
     * Close the open window if any.
     * @param context Graphics context handle
     */
    void CloseWindow(HContext context);

    /**
     * Iconify the open window if any.
     * @param context Graphics context handle
     */
    void IconifyWindow(HContext context);

    /**
     * Retrieve current state of the opened window, if any.
     * @param context Graphics context handle
     * @param state Aspect of the window state to query for
     * @return State of the supplied aspect. If no window is opened, 0 is always returned.
     */
    uint32_t GetWindowStateParam(HContext context, dmPlatform::WindowState state);

    /**
     * Returns the specified dpi of default monitor.
     *
     * @param context Graphics context handle
     * @return Specified dpi of the default display. If not supported, 0 is returned
     */
    uint32_t GetDisplayDpi(HContext context);

    /**
     * Returns the specified width of the opened window, which might differ from the actual window width.
     *
     * @param context Graphics context handle
     * @return Specified width of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetWidth(HContext context);

    /**
     * Returns the specified height of the opened window, which might differ from the actual window width.
     *
     * @param context Graphics context handle
     * @return Specified height of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetHeight(HContext context);

    /**
     * Return the width of the opened window, if any.
     * @param context Graphics context handle
     * @return Width of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetWindowWidth(HContext context);

    /**
     * Return the height of the opened window, if any.
     * @param context Graphics context handle
     * @return Height of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetWindowHeight(HContext context);

    /**
     * Set the size of the opened window, if any. If no window is opened, this function does nothing. If successfull,
     * the WindowResizeCallback will be called, if any was supplied when the window was opened.
     * @param context Graphics context handle
     * @param width New width of the window
     * @param height New height of the window
     */
    void SetWindowSize(HContext context, uint32_t width, uint32_t height);

    /**
     * Resizes a previously opened window. Only the window width and height will be changed with its framebuffer size
     * changed accordingly. The game width and height will be kept as specified from initial boot.
     * @param context Graphics context handle
     * @param width New width of the window
     * @param height New height of the window
     */
    void ResizeWindow(HContext context, uint32_t width, uint32_t height);

    /**
     * Get the scale factor of the display.
     * The display scale factor is usally 1.0 but will for instance be 2.0 on a macOS Retina display.
     * @return Scale factor
     */
    float GetDisplayScaleFactor(HContext context);

    /**
     * Return the default texture filtering modes.
     * @param context Graphics context handle
     * @param out_min_filter Out parameter to write the default min filtering mode to
     * @param out_mag_filter Out parameter to write the default mag filtering mode to
     */
    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter);

    /**
     * Begin frame rendering.
     *
     * @param context Graphics context handle
     */
    void BeginFrame(HContext context);

    /**
     * Flip screen buffers.
     *
     * @param context Graphics context handle
     */
    void Flip(HContext context);

    /**
     * Set buffer swap interval.
     * 1 for base frequency, eg every frame (60hz)
     * 2 for every second frame
     * etc
     * @param context Graphics context
     * @param swap_interval swap interval
     *
     */
    void SetSwapInterval(HContext context, uint32_t swap_interval);

    /**
     * Clear render target
     * @param context Graphics context
     * @param flags
     * @param red
     * @param green
     * @param blue
     * @param alpha
     * @param depth
     * @param stencil
     */
    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);

    bool     SetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset);
    void     EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, HProgram program);
    void     DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);
    void     HashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration);
    uint32_t GetVertexDeclarationStride(HVertexDeclaration vertex_declaration);

    void     EnableVertexBuffer(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index);
    void     DisableVertexBuffer(HContext context, HVertexBuffer vertex_buffer);

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);
    void DispatchCompute(HContext context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

    // Shaders
    HVertexProgram       NewVertexProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size);
    HFragmentProgram     NewFragmentProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size);
    HComputeProgram      NewComputeProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size);

    HProgram             NewProgram(HContext context, HComputeProgram compute_program);
    HProgram             NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program);
    void                 DeleteProgram(HContext context, HProgram program);

    bool                 ReloadVertexProgram(HVertexProgram prog, ShaderDesc* ddf);
    bool                 ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc* ddf);
    bool                 ReloadComputeProgram(HComputeProgram prog, ShaderDesc* ddf);
    void                 DeleteVertexProgram(HVertexProgram prog);
    void                 DeleteFragmentProgram(HFragmentProgram prog);
    void                 DeleteComputeProgram(HComputeProgram prog);

    bool                 IsShaderLanguageSupported(HContext _context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type);
    ShaderDesc::Language GetProgramLanguage(HProgram program);

    void                 EnableProgram(HContext context, HProgram program);
    void                 DisableProgram(HContext context);
    bool                 ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program);
    bool                 ReloadProgram(HContext context, HProgram program, HComputeProgram compute_program);

    // Attributes
    uint32_t         GetAttributeCount(HProgram prog);
    void             GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location);
    void             GetAttributeValues(const VertexAttribute& attribute, const uint8_t** data_ptr, uint32_t* data_size);
    Type             GetGraphicsType(VertexAttribute::DataType data_type);
    uint8_t*         WriteAttribute(const VertexAttributeInfos* attribute_infos, uint8_t* write_ptr, uint32_t vertex_index, const dmVMath::Matrix4* world_transform, const dmVMath::Point3& p, const dmVMath::Point3& p_local, const dmVMath::Vector4* color, float** uvs, uint32_t* page_indices, uint32_t num_textures);

    uint32_t         GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size);
    uint32_t         GetUniformCount(HProgram prog);
    HUniformLocation GetUniformLocation(HProgram prog, const char* name);

    void SetConstantV4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    void SetConstantM4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    void SetSampler(HContext context, HUniformLocation location, int32_t unit);
    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);

    void EnableState(HContext context, State state);
    void DisableState(HContext context, State state);
    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);
    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha);
    void SetDepthMask(HContext context, bool mask);
    void SetDepthFunc(HContext context, CompareFunc func);
    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);
    void SetStencilMask(HContext context, uint32_t mask);
    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask);
    void SetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask);
    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass);
    void SetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass);
    void SetCullFace(HContext context, FaceType face_type);
    void SetFaceWinding(HContext context, FaceWinding face_winding);
    void SetPolygonOffset(HContext context, float factor, float units);

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params);
    void          DeleteRenderTarget(HRenderTarget render_target);
    void          SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);
    HTexture      GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type);
    void          GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);
    void          SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height);
    uint32_t      GetBufferTypeIndex(BufferType buffer_type);
    BufferType    GetBufferTypeFromIndex(uint32_t index);
    const char*   GetBufferTypeLiteral(BufferType buffer_type);
    PipelineState GetPipelineState(HContext context);
    bool          IsContextFeatureSupported(HContext context, ContextFeature feature);

    TextureFormat GetSupportedCompressionFormat(HContext context, TextureFormat format, uint32_t width, uint32_t height);

    uint32_t GetTextureFormatBitsPerPixel(TextureFormat format);
    HTexture NewTexture(HContext context, const TextureCreationParams& params);
    void DeleteTexture(HTexture t);

    /**
     * Set texture data. For textures of type TEXTURE_TYPE_CUBE_MAP it's assumed that
     * 6 mip-maps are present contiguously in memory with stride m_DataSize
     *
     * @param texture HTexture
     * @param params TextureParams
     */
    void SetTexture(HTexture texture, const TextureParams& params);

    /**
     * Function called when a texture has been set asynchronously
     * @param user_data user data that will be passed to the SetTextureAsyncCallback
     */
    typedef void (*SetTextureAsyncCallback)(HTexture texture, void* user_data);

    /**
     * Set texture data asynchronously. For textures of type TEXTURE_TYPE_CUBE_MAP it's assumed that
     * 6 mip-maps are present contiguously in memory with stride m_DataSize
     *
     * @param texture HTexture
     * @param params TextureParams
     */
    void SetTextureAsync(HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data);

    void        SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
    uint32_t    GetTextureResourceSize(HTexture texture);
    uint16_t    GetTextureWidth(HTexture texture);
    uint16_t    GetTextureHeight(HTexture texture);
    uint16_t    GetTextureDepth(HTexture texture);
    uint16_t    GetOriginalTextureWidth(HTexture texture);
    uint16_t    GetOriginalTextureHeight(HTexture texture);
    uint8_t     GetTextureMipmapCount(HTexture texture);
    TextureType GetTextureType(HTexture texture);
    uint8_t     GetNumTextureHandles(HTexture texture);
    uint32_t    GetTextureUsageHintFlags(HTexture texture);

    const char* GetTextureTypeLiteral(TextureType texture_type);
    const char* GetTextureFormatLiteral(TextureFormat format);
    uint32_t    GetMaxTextureSize(HContext context);
    void        EnableTexture(HContext context, uint32_t unit, uint8_t id_index, HTexture texture);
    void        DisableTexture(HContext context, uint32_t unit, HTexture texture);

    // Calculating mipmap info helpers
    uint16_t    GetMipmapSize(uint16_t size_0, uint8_t mipmap);
    uint8_t     GetMipmapCount(uint16_t size);

    // Asset handle helpers
    const char* GetAssetTypeLiteral(AssetType type);
    bool        IsAssetHandleValid(HContext context, HAssetHandle asset_handle);

    static inline HAssetHandle MakeAssetHandle(HOpaqueHandle opaque_handle, AssetType asset_type)
    {
        assert(asset_type != ASSET_TYPE_NONE && "Invalid asset type");
        uint64_t handle = ((uint64_t) asset_type) << 32 | opaque_handle;
        assert(handle <= MAX_ASSET_HANDLE_VALUE);
        return handle;
    }

    static inline AssetType GetAssetType(HAssetHandle asset_handle)
    {
        return (AssetType) (asset_handle >> 32);
    }

    static inline HOpaqueHandle GetOpaqueHandle(HAssetHandle asset_handle)
    {
        return (HOpaqueHandle) asset_handle & 0xFFFFFFFF;
    }

    static inline bool IsColorBufferType(BufferType buffer_type)
    {
        return buffer_type == BUFFER_TYPE_COLOR0_BIT ||
               buffer_type == BUFFER_TYPE_COLOR1_BIT ||
               buffer_type == BUFFER_TYPE_COLOR2_BIT ||
               buffer_type == BUFFER_TYPE_COLOR3_BIT;
    }

    static inline bool HasLocalPositionAttribute(const VertexAttributeInfos& attribute_infos)
    {
        for (int i = 0; i < attribute_infos.m_NumInfos; ++i)
        {
            if (attribute_infos.m_Infos[i].m_SemanticType == VertexAttribute::SEMANTIC_TYPE_POSITION &&
                attribute_infos.m_Infos[i].m_CoordinateSpace == COORDINATE_SPACE_LOCAL)
            {
                return true;
            }
        }
        return false;
    }

    static inline bool IsTypeTextureType(Type type)
    {
        return type == TYPE_SAMPLER_2D ||
               type == TYPE_SAMPLER_CUBE ||
               type == TYPE_SAMPLER_2D_ARRAY ||
               type == TYPE_SAMPLER ||
               type == TYPE_TEXTURE_2D ||
               type == TYPE_TEXTURE_2D_ARRAY ||
               type == TYPE_TEXTURE_CUBE ||
               type == TYPE_IMAGE_2D;
    }

    static inline uint32_t GetLayerCount(TextureType type)
    {
        return type == TEXTURE_TYPE_CUBE_MAP ? 6 : 1;
    }

    /**
     * Get status of texture.
     *
     * @name GetTextureStatusFlags
     * @param texture HTexture
     * @return  TextureStatusFlags enumerated status bit flags
     */
    uint32_t GetTextureStatusFlags(HTexture texture);

    /** checks if the texture format is compressed
     * @name IsFormatTranscoded
     * @param format dmGraphics::TextureImage::CompressionType
     * @return true if the format is compressed
     */
    bool IsFormatTranscoded(TextureImage::CompressionType format);

    /** checks if the texture format is compressed
     * @name Transcode
     * @param path The path of the texture
     * @param image The input image
     * @param format The desired output format
     * @param images An array of transcoded mipmaps
     * @param sizes An array of transcoded mipmap sizes
     * @param num_transcoded_mips (in) the size of the input arrays, (out) the number of mipmaps stored in the arrays
     * @param format dmGraphics::TextureImage::CompressionType
     * @return true if the format is transcoded
     */
    bool Transcode(const char* path, TextureImage::Image* image, uint8_t image_count, TextureFormat format, uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips);

    /**
     * Read frame buffer pixels in BGRA format
     * @param buffer buffer to read to
     * @param buffer_size buffer size
     */
    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size);

    uint32_t    GetTypeSize(Type type);
    const char* GetGraphicsTypeLiteral(Type type);

    // Test functions:
    void* MapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access);
    bool  UnmapVertexBuffer(HContext context, HVertexBuffer buffer);
    void* MapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access);
    bool  UnmapIndexBuffer(HContext context, HIndexBuffer buffer);
}

#endif // DM_GRAPHICS_H
