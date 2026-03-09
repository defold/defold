#include "mouse_capture.h"
#include <windows.h>

#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif

namespace dmMouseCapture
{
    struct Context
    {
        HWND m_MessageWindow;
        bool m_Capturing;
        MouseDelta m_AccumulatedDelta;
        int m_SavedCursorX;
        int m_SavedCursorY;
    };

    static void ProcessRawInput(HContext context)
    {
        static constexpr size_t raw_input_buffer_size_in_u64s = (1024 * 6) / sizeof(uint64_t);
        uint64_t raw_input_buffer[raw_input_buffer_size_in_u64s] = {};
        MouseDelta frame_mouse_delta = {};

        while (true)
        {
            PRAWINPUT rawinput_buffer_ptr = reinterpret_cast<PRAWINPUT>(&raw_input_buffer);
            UINT out_buffer_size = sizeof(raw_input_buffer);
            UINT rawinput_count = 0;
            rawinput_count = GetRawInputBuffer(rawinput_buffer_ptr, &out_buffer_size, sizeof(RAWINPUTHEADER));
            if (rawinput_count == 0)
            {
                break;
            }
            if (rawinput_count == ~UINT(0))
            {
                break;
            }

            PRAWINPUT rawinput_ptr = rawinput_buffer_ptr;
            for (size_t i = 0; i < rawinput_count; i++)
            {
                switch (rawinput_ptr->header.dwType)
                {
                    case RIM_TYPEMOUSE:
                    {
                        if (rawinput_ptr->header.dwType == RIM_TYPEMOUSE && (rawinput_ptr->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                        {
                            frame_mouse_delta.dx += rawinput_ptr->data.mouse.lLastX;
                            frame_mouse_delta.dy += rawinput_ptr->data.mouse.lLastY;
                        }
                        break;
                    }
                }
                rawinput_ptr = NEXTRAWINPUTBLOCK(rawinput_ptr);
            }
        }

        context->m_AccumulatedDelta.dx += frame_mouse_delta.dx;
        context->m_AccumulatedDelta.dy += frame_mouse_delta.dy;
    }

    static LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INPUT)
        {
            // If we got some packets leaking into WM_INPUT I guess we can empty the queue
            Context* ctx = (Context*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (ctx)
            {
                ProcessRawInput(ctx);
            }
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    static bool EnsureWindowClass()
    {
        static bool registered = false;
        if (registered) return true;

        WNDCLASS wc = {};
        wc.lpfnWndProc = RawInputWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = TEXT("dmMouseCaptureRawInput");

        if (RegisterClass(&wc))
            registered = true;

        return registered;
    }

    HContext CreateContext(int save_cursor_x, int save_cursor_y)
    {
        Context* ctx = new Context();
        ctx->m_MessageWindow = NULL;
        ctx->m_Capturing = false;
        ctx->m_AccumulatedDelta.dx = 0.0;
        ctx->m_AccumulatedDelta.dy = 0.0;
        ctx->m_SavedCursorX = save_cursor_x;
        ctx->m_SavedCursorY = save_cursor_y;
        return ctx;
    }

    void WarpCursor(int x, int y)
    {
        SetCursorPos(x, y);
    }

    bool StartCapture(HContext context, WindowHandle window)
    {
        if (!context || context->m_Capturing)
            return false;

        if (!EnsureWindowClass())
            return false;

        POINT pt;
        GetCursorPos(&pt);

        context->m_MessageWindow = CreateWindowEx(
            0, TEXT("dmMouseCaptureRawInput"), NULL,
            0, 0, 0, 0, 0,
            HWND_MESSAGE,
            NULL, GetModuleHandle(NULL), NULL);

        if (!context->m_MessageWindow)
            return false;

        SetWindowLongPtr(context->m_MessageWindow, GWLP_USERDATA, (LONG_PTR)context);

        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x02;
        rid.dwFlags = 0;
        rid.hwndTarget = context->m_MessageWindow;

        if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
        {
            DestroyWindow(context->m_MessageWindow);
            context->m_MessageWindow = NULL;
            return false;
        }

        while (ShowCursor(FALSE) >= 0) {}
        RECT clipRect;
        clipRect.left = context->m_SavedCursorX;
        clipRect.right = context->m_SavedCursorX + 1;
        clipRect.top = context->m_SavedCursorY;
        clipRect.bottom = context->m_SavedCursorY + 1;
        ClipCursor(&clipRect);

        context->m_Capturing = true;
        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;
        return true;
    }

    void StopCapture(HContext context)
    {
        if (!context || !context->m_Capturing)
            return;

        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x02;
        rid.dwFlags = RIDEV_REMOVE;
        rid.hwndTarget = NULL;
        RegisterRawInputDevices(&rid, 1, sizeof(rid));

        if (context->m_MessageWindow)
        {
            DestroyWindow(context->m_MessageWindow);
            context->m_MessageWindow = NULL;
        }

        ClipCursor(NULL);
        while (ShowCursor(TRUE) < 0) {}

        context->m_Capturing = false;
    }

    bool PollDelta(HContext context, MouseDelta* out_delta)
    {
        if (!context || !context->m_Capturing || !out_delta)
            return false;

        ProcessRawInput(context);

        MSG msg;
        while (PeekMessage(&msg, context->m_MessageWindow, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        *out_delta = context->m_AccumulatedDelta;

        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        return (out_delta->dx != 0.0 || out_delta->dy != 0.0);
    }

    void DestroyContext(HContext context)
    {
        if (!context)
            return;

        if (context->m_Capturing)
            StopCapture(context);

        delete context;
    }
} // namespace dmMouseCapture
