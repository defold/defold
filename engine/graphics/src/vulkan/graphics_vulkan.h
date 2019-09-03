#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

namespace dmGraphics
{
    struct Context
    {
        uint32_t dummy : 31;
        uint32_t m_WindowOpened : 1;
    };

    struct Texture
    {
        uint32_t dummy;
    };

    struct VertexDeclaration
    {
        uint32_t dummy;
    };

    struct RenderTarget
    {
        uint32_t dummy;
    };
}
#endif // __GRAPHICS_DEVICE_VULKAN__

