///////////////////////////////////////////////////////////////////////////////////
//
//     graphics_device.h - Graphics device interface
//
///////////////////////////////////////////////////////////////////////////////////
#ifndef GRAPHICSDEVICE_H
#define GRAPHICSDEVICE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include "opengl/opengl_device_defines.h"

using namespace Vectormath::Aos;


namespace dmGraphics
{
    // primitive type
    enum PrimitiveType
    {
        PRIMITIVE_POINTLIST                     = GFXDEVICE_PRIMITIVE_POINTLIST,
        PRIMITIVE_LINES                         = GFXDEVICE_PRIMITIVE_LINES,
        PRIMITIVE_LINE_LOOP                     = GFXDEVICE_PRIMITIVE_LINE_LOOP,
        PRIMITIVE_LINE_STRIP                    = GFXDEVICE_PRIMITIVE_LINE_STRIP,
        PRIMITIVE_TRIANGLES                     = GFXDEVICE_PRIMITIVE_TRIANGLES,
        PRIMITIVE_TRIANGLE_STRIP                = GFXDEVICE_PRIMITIVE_TRIANGLE_STRIP,
        PRIMITIVE_TRIANGLE_FAN                  = GFXDEVICE_PRIMITIVE_TRIANGLE_FAN,
        PRIMITIVE_QUADS                         = GFXDEVICE_PRIMITIVE_QUADS,
        PRIMITIVE_QUAD_STRIP                    = GFXDEVICE_PRIMITIVE_QUAD_STRIP
    };

    // buffer clear types
    enum BufferClear
    {
        CLEAR_COLOUR_BUFFER                     = GFXDEVICE_CLEAR_COLOURUFFER,
        CLEAR_DEPTH_BUFFER                      = GFXDEVICE_CLEAR_DEPTHBUFFER,
        CLEAR_STENCIL_BUFFER                    = GFXDEVICE_CLEAR_STENCILBUFFER
    };

    // bool states
    enum RenderState
    {
        DEPTH_TEST                              = GFXDEVICE_STATE_DEPTH_TEST,
        ALPHA_TEST                              = GFXDEVICE_STATE_ALPHA_TEST,
        BLEND                                   = GFXDEVICE_STATE_BLEND,
        CULL_FACE                               = GFXDEVICE_STATE_CULL_FACE,
    };

    // Types
    enum Type
    {
        TYPE_BYTE                               = GFXDEVICE_TYPE_BYTE,
        TYPE_UNSIGNED_BYTE                      = GFXDEVICE_TYPE_UNSIGNED_BYTE,
        TYPE_SHORT                              = GFXDEVICE_TYPE_SHORT,
        TYPE_UNSIGNED_SHORT                     = GFXDEVICE_TYPE_UNSIGNED_SHORT,
        TYPE_INT                                = GFXDEVICE_TYPE_INT,
        TYPE_UNSIGNED_INT                       = GFXDEVICE_TYPE_UNSIGNED_INT,
        TYPE_FLOAT                              = GFXDEVICE_TYPE_FLOAT,
    };

    // Texture format
    enum TextureFormat
    {
        TEXTURE_FORMAT_LUMINANCE                = 0,
        TEXTURE_FORMAT_RGB                      = 1,
        TEXTURE_FORMAT_RGBA                     = 2,
        TEXTURE_FORMAT_RGB_DXT1                 = 3,
        TEXTURE_FORMAT_RGBA_DXT1                = 4,
        TEXTURE_FORMAT_RGBA_DXT3                = 5,
        TEXTURE_FORMAT_RGBA_DXT5                = 6,
        TEXTURE_FORMAT_DEPTH                    = 7
    };

    // Texture format
    enum TextureFilter
    {
        TEXTURE_FILTER_LINEAR = GFXDEVICE_TEXTURE_FILTER_LINEAR,
        TEXTURE_FILTER_NEAREST = GFXDEVICE_TEXTURE_FILTER_NEAREST
    };

    // Texture format
    enum TextureWrap
    {
        TEXTURE_WRAP_CLAMP = GFXDEVICE_TEXTURE_WRAP_CLAMP,
        TEXTURE_WRAP_CLAMP_TO_BORDER = GFXDEVICE_TEXTURE_WRAP_CLAMP_TO_BORDER,
        TEXTURE_WRAP_CLAMP_TO_EDGE = GFXDEVICE_TEXTURE_WRAP_CLAMP_TO_EDGE,
        TEXTURE_WRAP_MIRRORED_REPEAT = GFXDEVICE_TEXTURE_WRAP_MIRRORED_REPEAT,
        TEXTURE_WRAP_REPEAT = GFXDEVICE_TEXTURE_WRAP_REPEAT
    };

    // Blend factor
    enum BlendFactor
    {
        BLEND_FACTOR_ZERO                       = GFXDEVICE_BLEND_FACTOR_ZERO,
        BLEND_FACTOR_ONE                        = GFXDEVICE_BLEND_FACTOR_ONE,
        BLEND_FACTOR_SRC_COLOR                  = GFXDEVICE_BLEND_FACTOR_SRC_COLOR,
        BLEND_FACTOR_ONE_MINUS_SRC_COLOR        = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        BLEND_FACTOR_DST_COLOR                  = GFXDEVICE_BLEND_FACTOR_DST_COLOR,
        BLEND_FACTOR_ONE_MINUS_DST_COLOR        = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        BLEND_FACTOR_SRC_ALPHA                  = GFXDEVICE_BLEND_FACTOR_SRC_ALPHA,
        BLEND_FACTOR_ONE_MINUS_SRC_ALPHA        = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        BLEND_FACTOR_DST_ALPHA                  = GFXDEVICE_BLEND_FACTOR_DST_ALPHA,
        BLEND_FACTOR_ONE_MINUS_DST_ALPHA        = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        BLEND_FACTOR_SRC_ALPHA_SATURATE         = GFXDEVICE_BLEND_FACTOR_SRC_ALPHA_SATURATE,
        BLEND_FACTOR_CONSTANT_COLOR             = GFXDEVICE_BLEND_FACTOR_CONSTANT_COLOR,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR   = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
        BLEND_FACTOR_CONSTANT_ALPHA             = GFXDEVICE_BLEND_FACTOR_CONSTANT_ALPHA,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA   = GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    };

    enum BufferUsage
    {
        BUFFER_USAGE_STREAM_DRAW = GFXDEVICE_BUFFER_USAGE_STREAM_DRAW,
        BUFFER_USAGE_STREAM_READ = GFXDEVICE_BUFFER_USAGE_STREAM_READ,
        BUFFER_USAGE_STREAM_COPY = GFXDEVICE_BUFFER_USAGE_STREAM_COPY,
        BUFFER_USAGE_DYNAMIC_DRAW = GFXDEVICE_BUFFER_USAGE_DYNAMIC_DRAW,
        BUFFER_USAGE_DYNAMIC_READ = GFXDEVICE_BUFFER_USAGE_DYNAMIC_READ,
        BUFFER_USAGE_DYNAMIC_COPY = GFXDEVICE_BUFFER_USAGE_DYNAMIC_COPY,
        BUFFER_USAGE_STATIC_DRAW = GFXDEVICE_BUFFER_USAGE_STATIC_DRAW,
        BUFFER_USAGE_STATIC_READ = GFXDEVICE_BUFFER_USAGE_STATIC_READ,
        BUFFER_USAGE_STATIC_COPY = GFXDEVICE_BUFFER_USAGE_STATIC_COPY,
    };

    enum BufferAccess
    {
        BUFFER_ACCESS_READ_ONLY = GFXDEVICE_BUFFER_ACCESS_READ_ONLY,
        BUFFER_ACCESS_WRITE_ONLY = GFXDEVICE_BUFFER_ACCESS_WRITE_ONLY,
        BUFFER_ACCESS_READ_WRITE = GFXDEVICE_BUFFER_ACCESS_READ_WRITE,
    };

    enum MemoryType
    {
        MEMORY_TYPE_MAIN = 0,
    };

    enum WindowParam
    {
        WINDOW_PARAM_OPENED             = GFXDEVICE_OPENED,
        WINDOW_PARAM_ACTIVE             = GFXDEVICE_ACTIVE,
        WINDOW_PARAM_ICONIFIED          = GFXDEVICE_ICONIFIED,
        WINDOW_PARAM_ACCELERATED        = GFXDEVICE_ACCELERATED,
        WINDOW_PARAM_RED_BITS           = GFXDEVICE_RED_BITS,
        WINDOW_PARAM_GREEN_BITS         = GFXDEVICE_GREEN_BITS,
        WINDOW_PARAM_BLUE_BITS          = GFXDEVICE_BLUE_BITS,
        WINDOW_PARAM_ALPHA_BITS         = GFXDEVICE_ALPHA_BITS,
        WINDOW_PARAM_DEPTH_BITS         = GFXDEVICE_DEPTH_BITS,
        WINDOW_PARAM_STENCIL_BITS       = GFXDEVICE_STENCIL_BITS,
        WINDOW_PARAM_REFRESH_RATE       = GFXDEVICE_REFRESH_RATE,
        WINDOW_PARAM_ACCUM_RED_BITS     = GFXDEVICE_ACCUM_RED_BITS,
        WINDOW_PARAM_ACCUM_GREEN_BITS   = GFXDEVICE_ACCUM_GREEN_BITS,
        WINDOW_PARAM_ACCUM_BLUE_BITS    = GFXDEVICE_ACCUM_BLUE_BITS,
        WINDOW_PARAM_ACCUM_ALPHA_BITS   = GFXDEVICE_ACCUM_ALPHA_BITS,
        WINDOW_PARAM_AUX_BUFFERS        = GFXDEVICE_AUX_BUFFERS,
        WINDOW_PARAM_STEREO             = GFXDEVICE_STEREO,
        WINDOW_PARAM_WINDOW_NO_RESIZE   = GFXDEVICE_WINDOW_NO_RESIZE,
        WINDOW_PARAM_FSAA_SAMPLES       = GFXDEVICE_FSAA_SAMPLES
    };

    enum FaceType
    {
        FRONT           = GFXDEVICE_FACE_TYPE_FRONT,
        BACK            = GFXDEVICE_FACE_TYPE_BACK,
        FRONT_AND_BACK  = GFXDEVICE_FACE_TYPE_FRONT_AND_BACK
    };

    // Parameter structure for NewDevice
    struct CreateDeviceParams
    {
        uint32_t        m_DisplayWidth;
        uint32_t        m_DisplayHeight;
        const char*     m_AppTitle;
        bool            m_Fullscreen;
        bool            m_PrintDeviceInfo;
    };

    struct VertexElement
    {
        uint32_t        m_Stream;
        uint32_t        m_Size;
        Type            m_Type;
        uint32_t        m_Usage;
        uint32_t        m_UsageIndex;
    };

    struct TextureParams
    {
        TextureParams()
        : m_Format(TEXTURE_FORMAT_RGBA)
        , m_MinFilter(TEXTURE_FILTER_LINEAR)
        , m_MagFilter(TEXTURE_FILTER_LINEAR)
        , m_UWrap(TEXTURE_WRAP_REPEAT)
        , m_VWrap(TEXTURE_WRAP_REPEAT)
        , m_Data(0x0)
        , m_DataSize(0)
        , m_MipMap(0)
        , m_Width(0)
        , m_Height(0)
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
    };

    /**
     * Get context, soon to be deprecated
     * @return Current graphics context
     */
    HContext GetContext();

    /**
     * Create a new device
     * @param argc argc from main()
     * @param argv argb from main()
     * @param params Device parameters
     * @return A graphics device
     */
    HDevice NewDevice(int* argc, char** argv, CreateDeviceParams *params);

    /**
     * Destroy device
     */
    void DeleteDevice(HDevice device);

    /**
     * Flip screen buffers
     */
    void Flip();

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

    HVertexBuffer NewVertexBuffer(uint32_t size, const void* data, BufferUsage buffer_usage);
    void DeleteVertexBuffer(HVertexBuffer buffer);
    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access);
    bool UnmapVertexBuffer(HVertexBuffer buffer);

    HIndexBuffer NewIndexBuffer(uint32_t size, const void* data, BufferUsage buffer_usage);
    void DeleteIndexBuffer(HIndexBuffer buffer);
    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access);
    bool UnmapIndexBuffer(HIndexBuffer buffer);

    HVertexDeclaration NewVertexDeclaration(VertexElement* element, uint32_t count);
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer);
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);

    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer);
    void DisableVertexStream(HContext context, uint16_t stream);
    void DrawRangeElements(HContext context, PrimitiveType prim_type, uint32_t start, uint32_t count, Type type, HIndexBuffer index_buffer);
    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);

    HVertexProgram NewVertexProgram(const void* program, uint32_t program_size);
    HFragmentProgram NewFragmentProgram(const void* program, uint32_t program_size);
    void DeleteVertexProgram(HVertexProgram prog);
    void DeleteFragmentProgram(HFragmentProgram prog);
    void SetVertexProgram(HContext context, HVertexProgram program);
    void SetFragmentProgram(HContext context, HFragmentProgram program);

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register);
    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors);
    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors);

    void SetViewport(HContext context, int width, int height);

    void EnableState(HContext context, RenderState state);
    void DisableState(HContext context, RenderState state);
    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);
    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha);
    void SetDepthMask(HContext context, bool mask);
    void SetIndexMask(HContext context, uint32_t mask);
    void SetStencilMask(HContext context, uint32_t mask);
    void SetCullFace(HContext context, FaceType face_type);

    HRenderTarget NewRenderTarget(const TextureParams& params);
    void DeleteRenderTarget(HRenderTarget renderbuffer);
    void EnableRenderTarget(HContext context, HRenderTarget rendertarget);
    void DisableRenderTarget(HContext context, HRenderTarget rendertarget);
    HTexture GetRenderTargetTexture(HRenderTarget rendertarget);

    HTexture NewTexture(const TextureParams& params);
    void SetTexture(HTexture texture, const TextureParams& params);

    void DeleteTexture(HTexture t);
    void EnableTexture(HContext context, HTexture t);

    uint32_t GetWindowParam(WindowParam param);

    uint32_t GetWindowWidth();
    uint32_t GetWindowHeight();
}

#endif // GRAPHICSDEVICE_H
