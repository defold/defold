// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <dlib/time.h>
#include <dlib/log.h>

#include <glfw/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <glfw/glfw3native.h>

#include "platform_window_glfw3_private.h"
#include "platform_window_win32.h"

// Specified in engine.rc that is applied to the exe.
// A custom icon will replace the defold.ico file via the iconExe.java when bundling,
// so it should be fine as long as this number is the same as the entry specified in the .rc file!
#define IDI_APPICON 100

namespace dmPlatform
{
    HWND GetWindowsHWND(HWindow window)
    {
    	return glfwGetWin32Window(window->m_Window);
    }

    HGLRC GetWindowsHGLRC(HWindow window)
    {
    	return glfwGetWGLContext(window->m_Window);
    }

    static inline bool IsWindowForeground(HWindow window)
    {
        return GetWindowsHWND(window) == GetForegroundWindow();
    }

    static void RepackBGRPixels(uint32_t num_pixels, uint32_t bit_depth, uint8_t* pixels_in, uint8_t* pixels_out)
    {
        for(uint32_t px=0; px < num_pixels; px++)
        {
            if (bit_depth == 24)
            {
                pixels_out[0] = pixels_in[2];
                pixels_out[1] = pixels_in[1];
                pixels_out[2] = pixels_in[0];
                pixels_out[3] = 255;
                pixels_out+=4;
                pixels_in+=3;
            }
            else if (bit_depth == 32)
            {
                pixels_out[0] = pixels_in[2];
                pixels_out[1] = pixels_in[1];
                pixels_out[2] = pixels_in[0];
                pixels_out[3] = pixels_in[3];
                pixels_out+=4;
                pixels_in+=4;
            }
        }
    }

    static const char* HICONToGLFWImage(HICON hIcon, GLFWimage* image)
    {
        // Get icon information
        ICONINFO icon_info;
        if (!GetIconInfo(hIcon, &icon_info))
        {
            return "Unable to get icon information";
        }

        // Get bitmap information
        BITMAP bm;
        if (!GetObject(icon_info.hbmColor, sizeof(BITMAP), &bm))
        {
            DeleteObject(icon_info.hbmColor);
            DeleteObject(icon_info.hbmMask);
            return "Unable to get bitmap information";
        }

        if (!(bm.bmBitsPixel == 24 || bm.bmBitsPixel == 32))
        {
            DeleteObject(icon_info.hbmColor);
            DeleteObject(icon_info.hbmMask);
            return "Invalid bitmap depth. Only a bit depth of 24 or 32 is currently supported.";
        }

        BITMAPINFO bmi              = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = bm.bmWidth;
        bmi.bmiHeader.biHeight      = -bm.bmHeight; // Negative height for top-down bitmap
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = (WORD) bm.bmBitsPixel; // Use the actual color depth
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t row_size = ((bm.bmBitsPixel * bm.bmWidth + 31) / 32) * 4; // Row size in bytes
        uint8_t* pixels   = (uint8_t*) malloc(row_size * bm.bmHeight);

        HDC hdc = GetDC(NULL);
        if (!hdc)
        {
            free(pixels);
            DeleteObject(icon_info.hbmColor);
            DeleteObject(icon_info.hbmMask);
            return "Unable to get a device context";
        }

        // Get the original pixel data
        if (!GetDIBits(hdc, icon_info.hbmColor, 0, bm.bmHeight, pixels, &bmi, DIB_RGB_COLORS))
        {
            free(pixels);
            ReleaseDC(NULL, hdc);
            DeleteObject(icon_info.hbmColor);
            DeleteObject(icon_info.hbmMask);
            return "Unable to get the pixel data from the bitmap";
        }

        ReleaseDC(NULL, hdc);

        image->width = bm.bmWidth;
        image->height = bm.bmHeight;

        // Repack to RGBA. The bitmap is stored in BGR format, so we need this conversion regardless.
        // From the GLFW docs:
        // "The image data is 32-bit, little-endian, non-premultiplied RGBA, i.e. eight bits per channel with the red channel first.
        // The pixels are arranged canonically as sequential rows, starting from the top-left corner."
        uint8_t* pixels_rgba = (uint8_t*) malloc(bm.bmWidth * bm.bmHeight * 4);
        RepackBGRPixels(bm.bmWidth * bm.bmHeight, bm.bmBitsPixel, pixels, pixels_rgba);
        free(pixels);
        image->pixels = pixels_rgba;

        return NULL;
    }

    void SetWindowsIconNative(HWindow window)
    {
        HWND hwnd           = GetWindowsHWND(window);
        HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        HICON hIcon         = (HICON) LoadImageW(hInstance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

        if (!hIcon)
        {
            dmLogWarning("Unable to set windows application icon: No icon found!");
            return;
        }

        GLFWimage image = {};
        const char* err_msg_or_null = HICONToGLFWImage(hIcon, &image);

        if (err_msg_or_null)
        {
            dmLogWarning("Unable to set windows application icon: %s", err_msg_or_null);
            return;
        }

        glfwSetWindowIcon(window->m_Window, 1, &image);

        free(image.pixels);
    }

    void FocusWindowNative(HWindow window)
    {
        glfwFocusWindow(window->m_Window);

        // Windows doesn't always bring the window to front immediately when
        // the engine is rebooted. So we need to introduce a bit of lag here
        // to make sure our window will be on top eventually.
        uint32_t attempts = 0;
        const uint32_t attempts_max = 100;
        while(!IsWindowForeground(window) && attempts < attempts_max)
        {
            dmTime::Sleep(16000);
            attempts++;
            glfwFocusWindow(window->m_Window);
        }
    }

    void CenterWindowNative(HWindow wnd, GLFWmonitor* monitor)
    {
        if (!monitor)
            return;

        const GLFWvidmode* video_mode = glfwGetVideoMode(monitor);
        if (!video_mode)
            return;

        int32_t x = video_mode->width/2 - wnd->m_Width/2;
        int32_t y = video_mode->height/2 - wnd->m_Height/2;
        glfwSetWindowPos(wnd->m_Window, x, y);
    }
}
