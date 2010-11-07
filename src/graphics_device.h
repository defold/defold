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
        TEXTURE_FORMAT_RGBA_DXT5                = 6
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


    enum BufferType
    {
        BUFFER_TYPE_DYNAMIC = 0,
        BUFFER_TYPE_STATIC = 1
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

    // Parameter structure for CreateDevice
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
    HDevice CreateDevice(int* argc, char** argv, CreateDeviceParams *params);

    /**
     * Destroy device
     */
    void DestroyDevice();

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
    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer);

    HVertexBuffer NewVertexbuffer(uint32_t element_size, uint32_t element_count, BufferType buffer_type, MemoryType memory_type, uint32_t buffer_count, const void* data);
    void DeleteVertexBuffer(HVertexBuffer buffer);

    HIndexBuffer NewIndexBuffer(uint32_t element_count, BufferType buffer_type, MemoryType memory_type, const void* data);
    void DeleteIndexBuffer(HIndexBuffer buffer);

    HVertexDeclaration NewVertexDeclaration(VertexElement* element, uint32_t count);
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);
    void SetVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer);
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);



    void DisableVertexStream(HContext context, uint16_t stream);
    void DrawRangeElements(HContext context, PrimitiveType prim_type, uint32_t start, uint32_t count, Type type, HIndexBuffer index_buffer);
    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);

    HVertexProgram CreateVertexProgram(const void* program, uint32_t program_size);
    HFragmentProgram CreateFragmentProgram(const void* program, uint32_t program_size);
    void DestroyVertexProgram(HVertexProgram prog);
    void DestroyFragmentProgram(HFragmentProgram prog);
    void SetVertexProgram(HContext context, HVertexProgram program);
    void SetFragmentProgram(HContext context, HFragmentProgram program);

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register);
    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors);
    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors);


    void SetViewport(HContext context, int width, int height);

    void EnableState(HContext context, RenderState state);
    void DisableState(HContext context, RenderState state);
    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);
    void SetDepthMask(HContext context, bool mask);

    HTexture CreateTexture(uint32_t width, uint32_t height, TextureFormat texture_format);
    void SetTextureData(HTexture texture,
                           uint16_t mip_map,
                           uint16_t width, uint16_t height, uint16_t border,
                           TextureFormat texture_format, const void* data, uint32_t data_size);

    void DestroyTexture(HTexture t);
    void SetTexture(HContext context, HTexture t);

    uint32_t GetWindowParam(WindowParam param);

    uint32_t GetWindowWidth();
    uint32_t GetWindowHeight();
}

#endif // GRAPHICSDEVICE_H
