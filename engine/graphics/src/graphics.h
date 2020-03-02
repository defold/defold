#ifndef DM_GRAPHICS_H
#define DM_GRAPHICS_H

#include <stdint.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include <dmsdk/graphics/graphics.h>
#include <ddf/ddf.h>
#include <graphics/graphics_ddf.h>

namespace dmGraphics
{
    typedef uintptr_t                 HVertexProgram;
    typedef uintptr_t                 HFragmentProgram;
    typedef uintptr_t                 HProgram;
    typedef struct Context*           HContext;
    typedef struct Texture*           HTexture;
    typedef uintptr_t                 HVertexBuffer;
    typedef uintptr_t                 HIndexBuffer;
    typedef struct VertexDeclaration* HVertexDeclaration;
    typedef struct RenderTarget*      HRenderTarget;

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
    typedef void* (*EngineCreate)(int argc, char** argv);
    typedef void (*EngineDestroy)(void* engine);
    typedef int (*EngineUpdate)(void* engine);
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    static const HVertexProgram INVALID_VERTEX_PROGRAM_HANDLE = ~0u;
    static const HFragmentProgram INVALID_FRAGMENT_PROGRAM_HANDLE = ~0u;

    // primitive type
    enum PrimitiveType
    {
        PRIMITIVE_LINES          = 0,
        PRIMITIVE_TRIANGLES      = 1,
        PRIMITIVE_TRIANGLE_STRIP = 2,
    };

    // buffer clear types, each value is guaranteed to be separate bits
    enum BufferType
    {
        BUFFER_TYPE_COLOR_BIT   = 0x1,
        BUFFER_TYPE_DEPTH_BIT   = 0x2,
        BUFFER_TYPE_STENCIL_BIT = 0x4,
        MAX_BUFFER_TYPE_COUNT   = 3,
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

    // data types
    enum Type
    {
        TYPE_BYTE           = 0,
        TYPE_UNSIGNED_BYTE  = 1,
        TYPE_SHORT          = 2,
        TYPE_UNSIGNED_SHORT = 3,
        TYPE_INT            = 4,
        TYPE_UNSIGNED_INT   = 5,
        TYPE_FLOAT          = 6,
        TYPE_FLOAT_VEC4     = 7,
        TYPE_FLOAT_MAT4     = 8,
        TYPE_SAMPLER_2D     = 9,
        TYPE_SAMPLER_CUBE   = 10,
    };

    // Index buffer format
    enum IndexBufferFormat
    {
        INDEXBUFFER_FORMAT_16 = 0,
        INDEXBUFFER_FORMAT_32 = 1,
    };

    // Texture format
    enum TextureFormat
    {
        TEXTURE_FORMAT_LUMINANCE            = 0,
        TEXTURE_FORMAT_LUMINANCE_ALPHA      = 1,
        TEXTURE_FORMAT_RGB                  = 2,
        TEXTURE_FORMAT_RGBA                 = 3,
        TEXTURE_FORMAT_RGB_16BPP            = 4,
        TEXTURE_FORMAT_RGBA_16BPP           = 5,
        TEXTURE_FORMAT_RGB_DXT1             = 6,
        TEXTURE_FORMAT_RGBA_DXT1            = 7,
        TEXTURE_FORMAT_RGBA_DXT3            = 8,
        TEXTURE_FORMAT_RGBA_DXT5            = 9,
        TEXTURE_FORMAT_DEPTH                = 10,
        TEXTURE_FORMAT_STENCIL              = 11,
        TEXTURE_FORMAT_RGB_PVRTC_2BPPV1     = 12,
        TEXTURE_FORMAT_RGB_PVRTC_4BPPV1     = 13,
        TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1    = 14,
        TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1    = 15,
        TEXTURE_FORMAT_RGB_ETC1             = 16,

        // Floating point texture formats
        TEXTURE_FORMAT_RGB16F               = 17,
        TEXTURE_FORMAT_RGB32F               = 18,
        TEXTURE_FORMAT_RGBA16F              = 19,
        TEXTURE_FORMAT_RGBA32F              = 20,
        TEXTURE_FORMAT_R16F                 = 21,
        TEXTURE_FORMAT_RG16F                = 22,
        TEXTURE_FORMAT_R32F                 = 23,
        TEXTURE_FORMAT_RG32F                = 24,

        TEXTURE_FORMAT_COUNT
    };

    // Translation table to translate TextureFormat texture to BPP
    struct TextureFormatToBPP
    {
        uint8_t m_FormatToBPP[TEXTURE_FORMAT_COUNT];
        TextureFormatToBPP();
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

    // Blend factor
    enum BlendFactor
    {
        BLEND_FACTOR_ZERO                     = 0,
        BLEND_FACTOR_ONE                      = 0,
        BLEND_FACTOR_SRC_COLOR                = 0,
        BLEND_FACTOR_ONE_MINUS_SRC_COLOR      = 0,
        BLEND_FACTOR_DST_COLOR                = 0,
        BLEND_FACTOR_ONE_MINUS_DST_COLOR      = 0,
        BLEND_FACTOR_SRC_ALPHA                = 0,
        BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      = 0,
        BLEND_FACTOR_DST_ALPHA                = 0,
        BLEND_FACTOR_ONE_MINUS_DST_ALPHA      = 0,
        BLEND_FACTOR_SRC_ALPHA_SATURATE       = 0,
        BLEND_FACTOR_CONSTANT_COLOR           = 0,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 0,
        BLEND_FACTOR_CONSTANT_ALPHA           = 0,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 0,
    };

    // Compare func
    enum CompareFunc
    {
        COMPARE_FUNC_NEVER    = 0,
        COMPARE_FUNC_LESS     = 1,
        COMPARE_FUNC_LEQUAL   = 2,
        COMPARE_FUNC_GREATER  = 3,
        COMPARE_FUNC_GEQUAL   = 4,
        COMPARE_FUNC_EQUAL    = 5,
        COMPARE_FUNC_NOTEQUAL = 6,
        COMPARE_FUNC_ALWAYS   = 7,
    };

    // Stencil operation
    enum StencilOp
    {
        STENCIL_OP_KEEP      = 0,
        STENCIL_OP_ZERO      = 1,
        STENCIL_OP_REPLACE   = 2,
        STENCIL_OP_INCR      = 3,
        STENCIL_OP_INCR_WRAP = 4,
        STENCIL_OP_DECR      = 5,
        STENCIL_OP_DECR_WRAP = 6,
        STENCIL_OP_INVERT    = 7,
    };

    // Buffer usage
    enum BufferUsage
    {
        BUFFER_USAGE_STREAM_DRAW  = 0,
        BUFFER_USAGE_DYNAMIC_DRAW = 1,
        BUFFER_USAGE_STATIC_DRAW  = 2,
    };

    // Buffer access
    enum BufferAccess
    {
        BUFFER_ACCESS_READ_ONLY  = 0,
        BUFFER_ACCESS_WRITE_ONLY = 1,
        BUFFER_ACCESS_READ_WRITE = 2,
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
        TEXTURE_STATUS_OK           = 0,
        TEXTURE_STATUS_DATA_PENDING = (1 << 0),
    };

    struct VertexElement
    {
        const char*     m_Name;
        uint32_t        m_Stream;
        uint32_t        m_Size;
        Type            m_Type;
        bool            m_Normalize;
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
    };

    // Parameters structure for NewContext
    struct ContextParams
    {
        ContextParams();

        TextureFilter m_DefaultTextureMinFilter;
        TextureFilter m_DefaultTextureMagFilter;
        uint8_t       m_VerifyGraphicsCalls : 1;
        uint8_t       m_RenderDocSupport : 1;
        uint8_t       : 6;
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
     */
    bool Initialize();

    /**
     * Finalize graphics system
     */
    void Finalize();

    /**
     * Starts the app that needs to control the update loop (e.g iOS)
     */
    void AppBootstrap(int argc, char** argv, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);

    /**
     * Get the window refresh rate
     * @params context Graphics context handle
     * @return The window refresh rate, 0 if refresh rate ciuld not be read.
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

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    void DeleteVertexBuffer(HVertexBuffer buffer);
    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access);
    bool UnmapVertexBuffer(HVertexBuffer buffer);
    uint32_t GetMaxElementsVertices(HContext context);


    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    void DeleteIndexBuffer(HIndexBuffer buffer);
    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access);
    bool UnmapIndexBuffer(HIndexBuffer buffer);
    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format);
    uint32_t GetMaxElementsIndices(HContext context);

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count);
    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride);
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer);
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program);
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);

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

    uint32_t GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type);
    uint32_t GetUniformCount(HProgram prog);
    int32_t  GetUniformLocation(HProgram prog, const char* name);

    void SetConstantV4(HContext context, const Vectormath::Aos::Vector4* data, int base_register);
    void SetConstantM4(HContext context, const Vectormath::Aos::Vector4* data, int base_register);
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
    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass);
    void SetCullFace(HContext context, FaceType face_type);
    void SetPolygonOffset(HContext context, float factor, float units);

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT]);
    void DeleteRenderTarget(HRenderTarget render_target);
    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);
    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type);
    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);
    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height);
    inline uint32_t GetBufferTypeIndex(BufferType buffer_type);
    inline const char* GetBufferTypeLiteral(BufferType buffer_type);

    bool IsTextureFormatSupported(HContext context, TextureFormat format);
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

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap);
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
     * @param texture HTexture
     * @return  TextureStatusFlags enumerated status bit flags
     */
    uint32_t GetTextureStatusFlags(HTexture texture);

    /**
     * Read frame buffer pixels in BGRA format
     * @param buffer buffer to read to
     * @param buffer_size buffer size
     */
    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size);

    const char* GetBufferTypeLiteral(BufferType buffer_type)
    {
        if (buffer_type == BUFFER_TYPE_COLOR_BIT)
            return "BUFFER_TYPE_COLOR_BIT";
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT)
            return "BUFFER_TYPE_DEPTH_BIT";
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT)
            return "BUFFER_TYPE_STENCIL_BIT";
        else
            return "<unknown buffer type>";
    }

    uint32_t GetBufferTypeIndex(BufferType buffer_type)
    {
        if (buffer_type == BUFFER_TYPE_COLOR_BIT)
            return 0;
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT)
            return 1;
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT)
            return 2;
        else
            return ~0u;
    }

    /**
     * Iterates the application loop until it should be terminated.
     * @param user_data user data supplied to both the step and is running methods.
     * @param stepMethod the method to be used when executing a single iteration of the application loop.
     * @param isRunning the method used when determining whether iteration of the application loop should continue.
     */
    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running);
}

#endif // DM_GRAPHICS_H
