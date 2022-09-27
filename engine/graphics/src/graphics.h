// Copyright 2020-2022 The Defold Foundation
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
#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include <dmsdk/graphics/graphics.h>
#include <dlib/hash.h>
#include <ddf/ddf.h>
#include <graphics/graphics_ddf.h>

namespace dmGraphics
{

    typedef void (*WindowResizeCallback)(void* user_data, uint32_t width, uint32_t height);

    typedef void (*WindowFocusCallback)(void* user_data, uint32_t focus);

    typedef void (*WindowIconifyCallback)(void* user_data, uint32_t iconified);

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

    static const HVertexProgram INVALID_VERTEX_PROGRAM_HANDLE = ~0u;
    static const HFragmentProgram INVALID_FRAGMENT_PROGRAM_HANDLE = ~0u;

    enum AdapterType
    {
        ADAPTER_TYPE_NULL,
        ADAPTER_TYPE_OPENGL,
        ADAPTER_TYPE_VULKAN,
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

    static const uint8_t MAX_BUFFER_COLOR_ATTACHMENTS = 4;
    static const uint8_t MAX_BUFFER_TYPE_COUNT        = 2 + MAX_BUFFER_COLOR_ATTACHMENTS;

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

    // Translation table to translate RenderTargetAttachment to BufferType
    struct AttachmentToBufferType
    {
        BufferType m_AttachmentToBufferType[MAX_ATTACHMENT_COUNT];
        AttachmentToBufferType();
    };

    // Texture type
    enum TextureType
    {
        TEXTURE_TYPE_2D       = 0,
        TEXTURE_TYPE_CUBE_MAP = 1,
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

    enum MemoryType
    {
        MEMORY_TYPE_MAIN = 0,
    };

    enum WindowState
    {
        WINDOW_STATE_OPENED             = 0x00020001,
        WINDOW_STATE_ACTIVE             = 0x00020002,
        WINDOW_STATE_ICONIFIED          = 0x00020003,
        WINDOW_STATE_ACCELERATED        = 0x00020004,
        WINDOW_STATE_RED_BITS           = 0x00020005,
        WINDOW_STATE_GREEN_BITS         = 0x00020006,
        WINDOW_STATE_BLUE_BITS          = 0x00020007,
        WINDOW_STATE_ALPHA_BITS         = 0x00020008,
        WINDOW_STATE_DEPTH_BITS         = 0x00020009,
        WINDOW_STATE_STENCIL_BITS       = 0x0002000A,
        WINDOW_STATE_REFRESH_RATE       = 0x0002000B,
        WINDOW_STATE_ACCUM_RED_BITS     = 0x0002000C,
        WINDOW_STATE_ACCUM_GREEN_BITS   = 0x0002000D,
        WINDOW_STATE_ACCUM_BLUE_BITS    = 0x0002000E,
        WINDOW_STATE_ACCUM_ALPHA_BITS   = 0x0002000F,
        WINDOW_STATE_AUX_BUFFERS        = 0x00020010,
        WINDOW_STATE_STEREO             = 0x00020011,
        WINDOW_STATE_WINDOW_NO_RESIZE   = 0x00020012,
        WINDOW_STATE_FSAA_SAMPLES       = 0x00020013
    };

    enum WindowResult
    {
        WINDOW_RESULT_ALREADY_OPENED    = 1,
        WINDOW_RESULT_OK                = 0,
        WINDOW_RESULT_WINDOW_OPEN_ERROR = -2,
        WINDOW_RESULT_UNKNOWN_ERROR     = -1000,
    };

    enum TextureStatusFlags
    {
        TEXTURE_STATUS_OK               = 0,
        TEXTURE_STATUS_DATA_PENDING     = (1 << 0), // Currently waiting for the upload to be done
    };

    struct TextureCreationParams {

        TextureCreationParams() :
            m_Type(TEXTURE_TYPE_2D),
            m_Width(0),
            m_Height(0),
            m_OriginalWidth(0),
            m_OriginalHeight(0),
            m_MipMapCount(1)
        {}

        TextureType m_Type;
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint16_t    m_OriginalWidth;
        uint16_t    m_OriginalHeight;
        uint8_t     m_MipMapCount;
    };

    struct TextureParams
    {
        TextureParams()
        : m_Format(TEXTURE_FORMAT_RGBA)
        , m_MinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        , m_MagFilter(TEXTURE_FILTER_LINEAR)
        , m_UWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_VWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_Data(0x0)
        , m_DataSize(0)
        , m_MipMap(0)
        , m_Width(0)
        , m_Height(0)
        , m_SubUpdate(false)
        , m_X(0)
        , m_Y(0)
        {}

        TextureFormat m_Format;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap m_UWrap;
        TextureWrap m_VWrap;
        const void* m_Data;
        uint32_t m_DataSize;
        uint16_t m_MipMap;
        uint16_t m_Width;
        uint16_t m_Height;

        // For sub texture updates
        bool m_SubUpdate;
        uint32_t m_X;
        uint32_t m_Y;
    };

    // Parameters structure for OpenWindow
    struct WindowParams
    {
        WindowParams();

        /// Window resize callback
        WindowResizeCallback    m_ResizeCallback;
        /// User data supplied to the callback function
        void*                   m_ResizeCallbackUserData;
        /// Window close callback
        WindowCloseCallback     m_CloseCallback;
        /// User data supplied to the callback function
        void*                   m_CloseCallbackUserData;
        /// Window focus callback
        WindowFocusCallback     m_FocusCallback;
        /// User data supplied to the callback function
        void*                   m_FocusCallbackUserData;
        /// Window iconify callback
        WindowIconifyCallback   m_IconifyCallback;
        /// User data supplied to the callback function
        void*                   m_IconifyCallbackUserData;
        /// Window width, 640 by default
        uint32_t                m_Width;
        /// Window height, 480 by default
        uint32_t                m_Height;
        /// Number of samples (for multi-sampling), 1 by default
        uint32_t                m_Samples;
        /// Window title, "Dynamo App" by default
        const char*             m_Title;
        /// If the window should cover the full screen or not, false by default
        bool                    m_Fullscreen;
        /// Log info about the graphics device being used, false by default
        bool                    m_PrintDeviceInfo;

        bool                    m_HighDPI;

        // Window background color, RGB 0x00BBGGRR
        uint32_t                m_BackgroundColor;
    };

    // Parameters structure for NewContext
    struct ContextParams
    {
        ContextParams();

        TextureFilter m_DefaultTextureMinFilter;
        TextureFilter m_DefaultTextureMagFilter;
        uint32_t      m_GraphicsMemorySize;             // The max allowed Gfx memory (default 0)
        uint8_t       m_VerifyGraphicsCalls : 1;
        uint8_t       m_RenderDocSupport : 1;           // Vulkan only
        uint8_t       m_UseValidationLayers : 1;        // Vulkan only
        uint8_t       : 5;
    };

    const static uint8_t DM_GRAPHICS_STATE_WRITE_R = 0x1;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_G = 0x2;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_B = 0x4;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_A = 0x8;

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
     * Initialize graphics system
     * @params adapter_type_str String identifier for which adapter to use (vulkan/opengl/null)
     * @return True if a graphics backend could be created, false otherwise.
     */
    bool Initialize(const char* adapter_type_str = 0);

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
     * Open a window
     * @param context Graphics context handle
     * @param params Window parameters
     * @return The result of the operation
     */
    WindowResult OpenWindow(HContext context, WindowParams *params);

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
    uint32_t GetWindowState(HContext context, WindowState state);

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

    // Test functions:
    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access);
    bool UnmapVertexBuffer(HVertexBuffer buffer);
    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access);
    bool UnmapIndexBuffer(HIndexBuffer buffer);
    // <- end test functions

    bool SetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset);
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer);
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program);
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);
    void HashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration);

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);

    HVertexProgram NewVertexProgram(HContext context, ShaderDesc::Shader* ddf);
    HFragmentProgram NewFragmentProgram(HContext context, ShaderDesc::Shader* ddf);
    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program);
    void DeleteProgram(HContext context, HProgram program);

    bool ReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf);
    bool ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf);
    void DeleteVertexProgram(HVertexProgram prog);
    void DeleteFragmentProgram(HFragmentProgram prog);
    ShaderDesc::Language GetShaderProgramLanguage(HContext context);
    ShaderDesc::Shader* GetShaderProgram(HContext context, ShaderDesc* shader_desc);

    void EnableProgram(HContext context, HProgram program);
    void DisableProgram(HContext context);
    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program);

    uint32_t GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size);
    uint32_t GetUniformCount(HProgram prog);
    int32_t  GetUniformLocation(HProgram prog, const char* name);

    void SetConstantV4(HContext context, const Vectormath::Aos::Vector4* data, int count, int base_register);
    void SetConstantM4(HContext context, const Vectormath::Aos::Vector4* data, int count, int base_register);
    void SetSampler(HContext context, int32_t location, int32_t unit);

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

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT]);
    void DeleteRenderTarget(HRenderTarget render_target);
    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);
    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type);
    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);
    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height);
    inline uint32_t GetBufferTypeIndex(BufferType buffer_type);
    inline const char* GetBufferTypeLiteral(BufferType buffer_type);
    bool IsMultiTargetRenderingSupported(HContext context);
    PipelineState GetPipelineState(HContext context);

    TextureFormat GetSupportedCompressionFormat(HContext context, TextureFormat format, uint32_t width, uint32_t height);
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
     * Set texture data asynchronously. For textures of type TEXTURE_TYPE_CUBE_MAP it's assumed that
     * 6 mip-maps are present contiguously in memory with stride m_DataSize
     *
     * @param texture HTexture
     * @param params TextureParams
     */
    void SetTextureAsync(HTexture texture, const TextureParams& paramsa);

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
    uint32_t GetTextureResourceSize(HTexture texture);
    uint16_t GetTextureWidth(HTexture texture);
    uint16_t GetTextureHeight(HTexture texture);
    uint16_t GetOriginalTextureWidth(HTexture texture);
    uint16_t GetOriginalTextureHeight(HTexture texture);
    void EnableTexture(HContext context, uint32_t unit, HTexture texture);
    void DisableTexture(HContext context, uint32_t unit, HTexture texture);
    uint32_t GetMaxTextureSize(HContext context);

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
    bool IsFormatTranscoded(dmGraphics::TextureImage::CompressionType format);

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
    bool Transcode(const char* path, TextureImage::Image* image, TextureFormat format, uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips);

    /**
     * Read frame buffer pixels in BGRA format
     * @param buffer buffer to read to
     * @param buffer_size buffer size
     */
    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size);

    const char* GetBufferTypeLiteral(BufferType buffer_type)
    {
        switch(buffer_type)
        {
            case BUFFER_TYPE_COLOR0_BIT:  return "BUFFER_TYPE_COLOR_BIT";
            case BUFFER_TYPE_COLOR1_BIT:  return "BUFFER_TYPE_COLOR1_BIT";
            case BUFFER_TYPE_COLOR2_BIT:  return "BUFFER_TYPE_COLOR2_BIT";
            case BUFFER_TYPE_COLOR3_BIT:  return "BUFFER_TYPE_COLOR3_BIT";
            case BUFFER_TYPE_DEPTH_BIT:   return "BUFFER_TYPE_DEPTH_BIT";
            case BUFFER_TYPE_STENCIL_BIT: return "BUFFER_TYPE_STENCIL_BIT";
            default:break;
        }
        return "<unknown buffer type>";
    }

    uint32_t GetBufferTypeIndex(BufferType buffer_type)
    {
        switch(buffer_type)
        {
            case BUFFER_TYPE_COLOR0_BIT:  return 0;
            case BUFFER_TYPE_COLOR1_BIT:  return 1;
            case BUFFER_TYPE_COLOR2_BIT:  return 2;
            case BUFFER_TYPE_COLOR3_BIT:  return 3;
            case BUFFER_TYPE_DEPTH_BIT:   return 4;
            case BUFFER_TYPE_STENCIL_BIT: return 5;
            default: break;
        }
        return ~0u;
    }
}

#endif // DM_GRAPHICS_H
