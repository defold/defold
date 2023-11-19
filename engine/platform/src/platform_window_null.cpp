
#include <string>

#include "platform.h"

namespace dmPlatform
{
    struct Window
    {
        WindowParams m_CreateParams;
        uint32_t     m_WindowWidth;
        uint32_t     m_WindowHeight;
        uint32_t     m_WindowOpened : 1;
    };

    HWindow NewWindow(const WindowParams& params)
    {
        Window* wnd = new Window();
        memset(wnd, 0, sizeof(Window));
        wnd->m_CreateParams = params;
        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
    }

    PlatformResult OpenWindow(HWindow window)
    {
        window->m_WindowOpened = 1;
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
}
