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

#include "mouse_capture.h"
#include <windows.h>
#include <malloc.h>
#include <limits.h> // INT_MAX

#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif

namespace dmMouseCapture
{
    struct Context
    {
        HWND       m_MessageWindow;
        bool       m_Capturing;
        MouseDelta m_AccumulatedDelta;
        int        m_SavedCursorX;
        int        m_SavedCursorY;
    };

    static void ProcessRawInput(HContext context)
    {
        static constexpr size_t raw_input_buffer_size_in_u64s = (1024 * 6) / sizeof(uint64_t);
        uint64_t                raw_input_buffer[raw_input_buffer_size_in_u64s] = {};
        MouseDelta              frame_mouse_delta = {};

        while (true)
        {
            PRAWINPUT rawinput_buffer_ptr = (PRAWINPUT)&raw_input_buffer;
            UINT      out_buffer_size = sizeof(raw_input_buffer);
            UINT      rawinput_count = 0;
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

    // Encountered a bug where input packets seem to be dropping so the mouse would just hang, the likely cause is that
    // JavaFX was dispatching WM_INPUT events to our message window, so by adding this, we can catch the ones that were being
    // dropped, but we also only handle WM_INPUT messages because we dispatch other messages we don't care about when we PollDelta
    static LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INPUT)
        {
            Context* context = (Context*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            RAWINPUT raw = {};
            UINT     size = sizeof(raw);
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

            if (raw.header.dwType == RIM_TYPEMOUSE)
            {
                context->m_AccumulatedDelta.dx += raw.data.mouse.lLastX;
                context->m_AccumulatedDelta.dy += raw.data.mouse.lLastY;
            }
            ProcessRawInput(context);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    static bool EnsureWindowClass()
    {
        static bool registered = false;
        if (registered)
            return true;

        WNDCLASS wc = {};
        wc.lpfnWndProc = RawInputWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = TEXT("dmMouseCaptureRawInput");

        if (RegisterClass(&wc))
            registered = true;

        return registered;
    }

    void DestroyContext(HContext context)
    {
    }

    void WarpCursor(int x, int y)
    {
        SetCursorPos(x, y);
    }

    // Just in case we accumulated some deltas between the stop and start capture, let's clear them
    static void DiscardRawInput()
    {
        static constexpr size_t raw_input_buffer_size_in_u64s = (1024 * 6) / sizeof(uint64_t);
        uint64_t                raw_input_buffer[raw_input_buffer_size_in_u64s] = {};
        MouseDelta              frame_mouse_delta = {};

        while (true)
        {
            PRAWINPUT rawinput_buffer_ptr = (PRAWINPUT)&raw_input_buffer;
            UINT      out_buffer_size = sizeof(raw_input_buffer);
            UINT      rawinput_count = 0;
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
                rawinput_ptr = NEXTRAWINPUTBLOCK(rawinput_ptr);
            }
        }
    }

    HContext StartCapture(int save_cursor_x, int save_cursor_y)
    {
        if (!EnsureWindowClass())
            return nullptr;

        HWND hwnd = CreateWindowEx(0, TEXT("dmMouseCaptureRawInput"), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

        if (!hwnd)
            return nullptr;

        Context* context = new Context();
        context->m_MessageWindow = hwnd;
        context->m_Capturing = true;
        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;
        context->m_SavedCursorX = save_cursor_x;
        context->m_SavedCursorY = save_cursor_y;

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)context);

        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x02;
        rid.dwFlags = 0;
        rid.hwndTarget = hwnd;

        if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
        {
            DestroyWindow(hwnd);
            delete context;
            return nullptr;
        }

        while (ShowCursor(FALSE) >= 0)
        {
        }
        RECT clipRect;
        clipRect.left = context->m_SavedCursorX;
        clipRect.right = context->m_SavedCursorX + 1;
        clipRect.top = context->m_SavedCursorY;
        clipRect.bottom = context->m_SavedCursorY + 1;
        ClipCursor(&clipRect);

        DiscardRawInput();

        return context;
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
        }

        ClipCursor(NULL);
        while (ShowCursor(TRUE) < 0)
        {
        }

        delete context;
    }

    // In order to drain all messages from the queue while still keeping WM_INPUT, we call PeekMessage with PM_REMOVE,
    // but we can only provide a min and max range of the kinds of messages we want to drain. so for the arguments to
    // wMsgFilterMin and wMsgFilterMax we first provide 0 up to before WM_INPUT, and then from WM_INPUT + 1 . A small
    // optimization here sets exhausted to true when the first range gets exhausted so we don't check that range again
    // while clearing the second range. This might mean that messages can arrive in the first range while we're clearing
    // the second range out, but those should be handled during the next PollDelta.
    static inline bool ClearNonWMInputMessages(MSG* msg, bool& exhausted, HContext context)
    {
        if (!exhausted)
        {
            if (PeekMessage(msg, context->m_MessageWindow, 0, WM_INPUT - 1, PM_REMOVE))
            {
                return true;
            }
        }
        exhausted = true;
        if (PeekMessage(msg, context->m_MessageWindow, WM_INPUT + 1, UINT_MAX, PM_REMOVE))
        {
            return true;
        }
        return false;
    }

    bool PollDelta(HContext context, MouseDelta* out_delta)
    {
        if (!context || !context->m_Capturing || !out_delta)
            return false;

        ProcessRawInput(context);

        bool exhausted = false;
        MSG  msg;
        // We might have accumulated other messages we are not interested in handling, so clear
        // them out so we don't crash the app
        while (ClearNonWMInputMessages(&msg, exhausted, context))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        *out_delta = context->m_AccumulatedDelta;

        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        return (out_delta->dx != 0.0 || out_delta->dy != 0.0);
    }
} // namespace dmMouseCapture
