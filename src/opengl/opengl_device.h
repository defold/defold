#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <vectormath/cpp/vectormath_aos.h>
#include "opengl_device_defines.h"
#include <sdl/SDL.h>

namespace dmGraphics
{
    struct Texture
    {
        GLuint      m_Texture;
    };

    struct Device
    {
        SDL_Surface*    m_SDLscreen;

        struct
        {
            uint32_t    m_DisplayWidth;
            uint32_t    m_DisplayHeight;
        };

    };


    struct Context
    {
        Vectormath::Aos::Matrix4 m_ViewMatrix;
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
        uint32_t    m_ElementSize;
        uint32_t    m_ElementCount;
        BufferType  m_BufferType;
        MemoryType  m_MemoryType;
        uint32_t    m_BufferCount;
        void*       m_Data[3];
        GLuint      m_VboId;

    };

    struct IndexBuffer
    {
        uint32_t    m_ElementCount;
        BufferType  m_BufferType;
        MemoryType  m_MemoryType;
        void*       m_Data;
        GLuint      m_VboId;
    };
}
#endif // __GRAPHICS_DEVICE_OPENGL__

