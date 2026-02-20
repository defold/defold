#include "mouse_capture.h"
#include <windows.h>

#include <windows.h>
#include "mouse_capture.h"

namespace dmMouseCapture
{
    struct Context
    {
        HWND m_MessageWindow;
        bool m_Capturing;
        MouseDelta m_AccumulatedDelta;
    };

    static LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INPUT)
        {
            Context* ctx = (Context*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (ctx)
            {
                UINT size = 0;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
                if (size > 0)
                {
                    BYTE* buf = (BYTE*)_alloca(size);
                    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) == size)
                    {
                        RAWINPUT* raw = (RAWINPUT*)buf;
                        if (raw->header.dwType == RIM_TYPEMOUSE &&
                            (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                        {
                            ctx->m_AccumulatedDelta.dx += raw->data.mouse.lLastX;
                            ctx->m_AccumulatedDelta.dy += raw->data.mouse.lLastY;
                        }
                    }
                }
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

    HContext CreateContext()
    {
        Context* ctx = new Context();
        ctx->m_MessageWindow = NULL;
        ctx->m_Capturing = false;
        ctx->m_AccumulatedDelta.dx = 0.0;
        ctx->m_AccumulatedDelta.dy = 0.0;
        return ctx;
    }

    bool StartCapture(HContext context, HWND window)
    {
        if (!context || context->m_Capturing)
            return false;

        if (!EnsureWindowClass())
            return false;

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

        context->m_Capturing = false;
    }

    bool PollDelta(HContext context, MouseDelta* out_delta)
    {
        if (!context || !context->m_Capturing || !out_delta)
            return false;

        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        MSG msg;
        while (PeekMessage(&msg, context->m_MessageWindow, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        *out_delta = context->m_AccumulatedDelta;
        return true;
    }

    void DestroyContext(HContext context)
    {
        if (!context)
            return;

        if (context->m_Capturing)
            StopCapture(context);

        delete context;
    }
}

extern "C" {
    __declspec(dllexport) void* MouseCapture_CreateContext() {
        return dmMouseCapture::CreateContext();
    }

    __declspec(dllexport) void MouseCapture_DestroyContext(void* context) {
        dmMouseCapture::DestroyContext((dmMouseCapture::HContext)context);
    }

    __declspec(dllexport) bool MouseCapture_StartCapture(void* context, HWND window) {
        return dmMouseCapture::StartCapture((dmMouseCapture::HContext)context, window);
    }

    __declspec(dllexport) void MouseCapture_StopCapture(void* context) {
        dmMouseCapture::StopCapture((dmMouseCapture::HContext)context);
    }

    __declspec(dllexport) bool MouseCapture_PollDelta(void* context, dmMouseCapture::MouseDelta* delta) {
        return dmMouseCapture::PollDelta((dmMouseCapture::HContext)context, delta);
    }
}
