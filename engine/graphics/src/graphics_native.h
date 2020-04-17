#ifndef DM_GRAPHICS_NATIVE_H
#define DM_GRAPHICS_NATIVE_H

#include <dmsdk/graphics/graphics_native.h>

namespace dmGraphics
{
	bool NativeInit(const ContextParams& params);
	void NativeExit();

	// get the window size directly from the window manager
    void GetNativeWindowSize(uint32_t* width, uint32_t* height);

    void SwapBuffers();

    void SetSwapInterval(HContext context, uint32_t swap_interval);
}

#endif // DM_GRAPHICS_NATIVE_H
