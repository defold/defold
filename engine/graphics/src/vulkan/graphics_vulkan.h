#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

#include <vulkan/vulkan.h>

namespace dmGraphics
{
    struct Context
    {
        VkInstance   m_Instance;
        VkSurfaceKHR m_WindowSurface;
        uint32_t     m_Padding      : 31;
        uint32_t     m_WindowOpened : 1;
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

    VkResult CreateVkInstance(VkInstance* vkInstanceOut, bool enableValidation);

    // Implemented per supported platform
    VkResult CreateVKWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, bool enableHighDPI);
}
#endif // __GRAPHICS_DEVICE_VULKAN__

