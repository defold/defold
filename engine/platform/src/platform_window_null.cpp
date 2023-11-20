
#include <string>

#include "platform.h"

namespace dmPlatform
{
    struct Window
    {
        WindowParams m_CreateParams;
        bool         m_DeviceStates[DEVICE_STATE_KEYBOARD_COUNT];
        uint32_t     m_WindowWidth;
        uint32_t     m_WindowHeight;
        uint32_t     m_WindowOpened : 1;
    };

    HWindow NewWindow()
    {
        Window* wnd = new Window();
        memset(wnd, 0, sizeof(Window));
        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
    }

    PlatformResult OpenWindow(HWindow window, const WindowParams& params)
    {
        if (window->m_WindowOpened)
        {
            return PLATFORM_RESULT_WINDOW_ALREADY_OPENED;
        }

        window->m_CreateParams = params;
        window->m_WindowOpened = 1;
        window->m_WindowWidth  = params.m_Width;
        window->m_WindowHeight = params.m_Height;

        return PLATFORM_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        window->m_WindowOpened = 0;
        window->m_WindowWidth  = 0;
        window->m_WindowHeight = 0;
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_WindowHeight;
    }

    uint32_t GetWindowState(HWindow window, WindowState state)
    {
        if (state == WINDOW_STATE_OPENED)
        {
            return window->m_WindowOpened;
        }
        return 0;
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return 1.0f;
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        window->m_WindowWidth  = width;
        window->m_WindowHeight = height;

        if (window->m_CreateParams.m_ResizeCallback)
        {
            window->m_CreateParams.m_ResizeCallback(window->m_CreateParams.m_ResizeCallbackUserData, width, height);
        }
    }

    void SetSwapInterval(HWindow window, uint32_t swap_interval)
    {}

    void IconifyWindow(HWindow window)
    {}

    void PollEvents(HWindow window)
    {}

    void SetDeviceState(HWindow window, DeviceState state, bool op1)
    {
        SetDeviceState(window, state, op1, false);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1, bool op2)
    {
        window->m_DeviceStates[(int) state] = op1;
    }

    bool GetDeviceState(HWindow window, DeviceState state)
    {
        return window->m_DeviceStates[(int) state];
    }
}
