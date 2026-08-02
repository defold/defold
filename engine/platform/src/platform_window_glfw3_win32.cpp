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
#include <dlib/dstrings.h>

#include <stdint.h>
#include <string.h>

#include <glfw/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <glfw/glfw3native.h>

#include <xinput.h>

#include "platform_window_glfw3_private.h"
#include "platform_window_win32.h"

// Specified in engine.rc that is applied to the exe.
// A custom icon will replace the defold.ico file via the iconExe.java when bundling,
// so it should be fine as long as this number is the same as the entry specified in the .rc file!
#define IDI_APPICON 100

namespace dmPlatform
{
    struct XInputCapabilitiesEx
    {
        XINPUT_CAPABILITIES m_Capabilities;
        WORD                m_VendorId;
        WORD                m_ProductId;
        WORD                m_ProductVersion;
        WORD                m_Reserved;
        DWORD               m_Reserved2;
    };

    typedef DWORD (WINAPI *XInputGetCapabilitiesFn)(DWORD user_index, DWORD flags, XINPUT_CAPABILITIES* capabilities);
    typedef DWORD (WINAPI *XInputGetCapabilitiesExFn)(DWORD reserved, DWORD user_index, DWORD flags, XInputCapabilitiesEx* capabilities);

    static XInputGetCapabilitiesFn g_XInputGetCapabilities = 0;
    static XInputGetCapabilitiesExFn g_XInputGetCapabilitiesEx = 0;
    static bool g_XInputCapabilitiesFunctionsResolved = false;
    static char g_JoystickDeviceGuid[GLFW_JOYSTICK_LAST + 1][33];
    static HWND g_ConsoleCloseWindow = 0;

    static BOOL WINAPI ConsoleControlHandler(DWORD control_type)
    {
        switch (control_type)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            if (g_ConsoleCloseWindow)
            {
                PostMessageW(g_ConsoleCloseWindow, WM_CLOSE, 0, 0);
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
        }
    }

    void InstallWindowCloseHandlerNative(HWindow window)
    {
        g_ConsoleCloseWindow = glfwGetWin32Window(window->m_Window);
        SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
    }

    void UninstallWindowCloseHandlerNative(HWindow window)
    {
        if (g_ConsoleCloseWindow != glfwGetWin32Window(window->m_Window))
        {
            return;
        }

        SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);
        g_ConsoleCloseWindow = 0;
    }

    static bool IsGLFWXInputGuid(const char* guid)
    {
        return guid && strncmp(guid, "78696e707574", 12) == 0 && strlen(guid) == 32;
    }

    static void ResolveXInputCapabilitiesFunctions()
    {
        if (g_XInputCapabilitiesFunctionsResolved)
        {
            return;
        }

        g_XInputCapabilitiesFunctionsResolved = true;

        const wchar_t* libraries[] = {
            // XInputGetCapabilitiesEx is available by ordinal from the
            // Windows 8+ XInput implementation.  The engine links against
            // xinput9_1_0, but that compatibility DLL does not export it.
            // https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-versions
            L"xinput1_4.dll",
            L"xinput1_3.dll",
            L"xinput9_1_0.dll",
        };

        for (uint32_t i = 0; i < sizeof(libraries) / sizeof(libraries[0]); ++i)
        {
            HMODULE xinput_module = GetModuleHandleW(libraries[i]);
            if (!xinput_module)
            {
                // GLFW loads XInput dynamically and the selected DLL is not
                // guaranteed to still be discoverable by name here.  Load a
                // suitable implementation explicitly and retain it for the
                // lifetime of the process so the resolved pointers stay valid.
                xinput_module = LoadLibraryW(libraries[i]);
            }
            if (!xinput_module)
            {
                continue;
            }

            XInputGetCapabilitiesFn get_capabilities = (XInputGetCapabilitiesFn) GetProcAddress(xinput_module, "XInputGetCapabilities");
            XInputGetCapabilitiesExFn get_capabilities_ex = (XInputGetCapabilitiesExFn) GetProcAddress(xinput_module, (LPCSTR) 108);
            if (get_capabilities && get_capabilities_ex)
            {
                g_XInputGetCapabilities = get_capabilities;
                g_XInputGetCapabilitiesEx = get_capabilities_ex;
                return;
            }
        }
    }

    static bool GetConnectedXInputIndexByJoystickIndex(uint32_t joystick_index, DWORD* xinput_index)
    {
        uint32_t connected_xinput_count = 0;
        for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index)
        {
            XINPUT_CAPABILITIES capabilities = {};
            if (g_XInputGetCapabilities(index, 0, &capabilities) != ERROR_SUCCESS)
            {
                continue;
            }

            if (connected_xinput_count == joystick_index)
            {
                *xinput_index = index;
                return true;
            }
            ++connected_xinput_count;
        }

        return false;
    }

    const char* GetJoystickDeviceGuidNative(HWindow, uint32_t joystick_index, const char* glfw_guid)
    {
        if (!IsGLFWXInputGuid(glfw_guid) || joystick_index > GLFW_JOYSTICK_LAST)
        {
            return 0;
        }

        ResolveXInputCapabilitiesFunctions();
        if (!g_XInputGetCapabilities || !g_XInputGetCapabilitiesEx)
        {
            return 0;
        }

        DWORD xinput_index = 0;
        if (!GetConnectedXInputIndexByJoystickIndex(joystick_index, &xinput_index))
        {
            return 0;
        }

        XInputCapabilitiesEx capabilities = {};
        if (g_XInputGetCapabilitiesEx(1, xinput_index, 0, &capabilities) != ERROR_SUCCESS)
        {
            return 0;
        }

        if (capabilities.m_VendorId == 0 || capabilities.m_ProductId == 0)
        {
            return 0;
        }

        dmSnPrintf(g_JoystickDeviceGuid[joystick_index], sizeof(g_JoystickDeviceGuid[joystick_index]),
                   "03000000%02x%02x0000%02x%02x000000000000",
                   (uint8_t) (capabilities.m_VendorId & 0xff),
                   (uint8_t) (capabilities.m_VendorId >> 8),
                   (uint8_t) (capabilities.m_ProductId & 0xff),
                   (uint8_t) (capabilities.m_ProductId >> 8));

        return g_JoystickDeviceGuid[joystick_index];
    }

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

    void SetWindowedFullscreenFocusNative(HWindow, bool)
    {
        // NOP
    }

    void SetWindowedSizeFromSettingsNative(HWindow, int32_t, int32_t)
    {
        // NOP
    }

    void SetFullscreenWindowModeParamsNative(GLFWmonitor* monitor, const GLFWvidmode* mode, WindowModeParams* mode_params)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwGetMonitorPos(monitor, &mode_params->m_X, &mode_params->m_Y);
        mode_params->m_Width              = mode->width;
        mode_params->m_Height             = mode->height;
        mode_params->m_WindowedFullscreen = true;
    }

    bool CanSetOpenGLCoreProfileHintNative(bool use_highest_version)
    {
        // Not supported on Windows when requesting the default OpenGL version, which will use the highest available version.
        return !use_highest_version;
    }
}
