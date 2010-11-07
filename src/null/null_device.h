#ifndef GRAPHICS_DEVICE_NULL
#define GRAPHICS_DEVICE_NULL

#include "null_device_defines.h"

namespace dmGraphics
{
    struct Texture
    {
        GLuint      m_Texture;
    };

    struct Device
    {
        struct
        {
            uint32_t    m_DisplayWidth;
            uint32_t    m_DisplayHeight;
        };
        void*    m_SDLscreen;
        uint32_t m_Opened : 1;
    };

    struct Context
    {
        Vectormath::Aos::Matrix4 m_ViewMatrix;
    };

    struct VertexDeclaration
    {
    };

    struct VertexBuffer
    {
    };

    struct IndexBuffer
    {
    };

}

#endif // __GRAPHICS_DEVICE_NULL__
