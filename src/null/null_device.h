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
        void*    m_SDLscreen;

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
    };

    struct VertexBuffer
    {
    };

    struct IndexBuffer
    {
    };

}

#endif // __GRAPHICS_DEVICE_NULL__
