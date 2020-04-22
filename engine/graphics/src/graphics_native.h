#ifndef DM_GRAPHICS_NATIVE_H
#define DM_GRAPHICS_NATIVE_H

#include <dmsdk/graphics/graphics_native.h>
#include <graphics/graphics.h>

namespace dmGraphics
{
	bool NativeInit(const ContextParams& params);
	void NativeExit();

    void SwapBuffers();

    void SetSwapInterval(HContext context, uint32_t swap_interval);

    WindowResult VulkanOpenWindow(HContext context, WindowParams* params);
    void VulkanCloseWindow(HContext context);
    uint32_t VulkanGetDisplayDpi(HContext context);
    uint32_t VulkanGetWidth(HContext context);
    uint32_t VulkanGetHeight(HContext context);
    uint32_t VulkanGetWindowWidth(HContext context);
    uint32_t VulkanGetWindowHeight(HContext context);
    uint32_t VulkanGetWindowRefreshRate(HContext context);
    void VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void VulkanResizeWindow(HContext context, uint32_t width, uint32_t height);
    void VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void VulkanGetNativeWindowSize(uint32_t* width, uint32_t* height);
    void VulkanIconifyWindow(HContext context);
    uint32_t VulkanGetWindowState(HContext context, WindowState state);
}

#endif // DM_GRAPHICS_NATIVE_H
