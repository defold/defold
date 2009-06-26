#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include "opengl_device_defines.h"
#include <sdl/SDL.h>

namespace Graphics
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

}
#endif // __GRAPHICS_DEVICE_OPENGL__

