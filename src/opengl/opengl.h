#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <vectormath/cpp/vectormath_aos.h>
#include "opengl_defines.h"
#include <sdl/SDL.h>

namespace dmGraphics
{
    struct Context
    {
        uint32_t    m_WindowWidth;
        uint32_t    m_WindowHeight;
    };

    struct Texture
    {
        GLuint      m_Texture;
    };

    struct VertexDeclaration
    {
        struct Stream
        {
            uint32_t    m_Index;
            uint32_t    m_Size;
            uint32_t    m_Usage;
            Type        m_Type;
            uint32_t    m_UsageIndex;
            uint32_t    m_Offset;
        };

        Stream      m_Streams[8];
        uint32_t    m_StreamCount;
        uint32_t    m_Stride;

    };
    struct VertexBuffer
    {
        GLuint      m_VboId;
    };

    struct IndexBuffer
    {
        GLuint      m_VboId;
    };

    struct RenderTarget
    {
        GLuint      m_Id;
        HTexture    m_BufferTextures[MAX_BUFFER_TYPE_COUNT];
    };

}
#endif // __GRAPHICS_DEVICE_OPENGL__

