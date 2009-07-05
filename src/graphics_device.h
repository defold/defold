///////////////////////////////////////////////////////////////////////////////////
//
//	graphics_device.h - Graphics device interface
//
///////////////////////////////////////////////////////////////////////////////////
#ifndef __GRAPHICSDEVICE_H__
#define __GRAPHICSDEVICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include "opengl/opengl_device_defines.h"

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
        PRIMITIVE_TRIANGLE_FAN                  = GFXDEVICE_PRIMITIVE_TRIANGLE_FAN
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

    enum MatrixMode
    {
        MATRIX_TYPE_WORLD                       = GFXDEVICE_MATRIX_TYPE_WORLD,
        MATRIX_TYPE_VIEW                        = GFXDEVICE_MATRIX_TYPE_VIEW,
        MATRIX_TYPE_PROJECTION                  = GFXDEVICE_MATRIX_TYPE_PROJECTION
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

    // Parameter structure for CreateDevice
    struct CreateDeviceParams
    {
        uint32_t        m_DisplayWidth;
        uint32_t        m_DisplayHeight;
        const char*	    m_AppTitle;
        bool            m_Fullscreen;
        bool            m_PrintDeviceInfo;
    };

    HContext GetContext();

    HDevice CreateDevice(int* argc, char** argv, CreateDeviceParams *params);
    void DestroyDevice();

    // clear/draw functions
    void Flip();
    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);
    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer);
    void DisableVertexStream(HContext context, uint16_t stream);
    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);

    HVertexProgram CreateVertexProgram(const void* program, uint32_t program_size);
    HFragmentProgram CreateFragmentProgram(const void* program, uint32_t program_size);
    void DestroyVertexProgram(HVertexProgram prog);
    void DestroyFragmentProgram(HFragmentProgram prog);
    void SetVertexProgram(HContext context, HVertexProgram program);
    void SetFragmentProgram(HContext context, HFragmentProgram program);

    void SetFragmentConstant(HContext context, const Vectormath::Aos::Vector4* data, int base_register);
    void SetVertexConstantBlock(HContext context, const Vectormath::Aos::Vector4* data, int base_register, int num_vectors);
    void SetFragmentConstantBlock(HContext context, const Vectormath::Aos::Vector4* data, int base_register, int num_vectors);


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



}

#endif	// __GRAPHICSDEVICE_H__
