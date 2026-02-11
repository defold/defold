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

#include "window.hpp"

#include <string.h>

extern "C"
{
    void WindowCreateParamsInitialize(WindowCreateParams* params)
    {
        if (!params)
        {
            return;
        }

        memset(params, 0, sizeof(*params));
        params->m_Width   = 640;
        params->m_Height  = 480;
        params->m_Samples = 1;
        params->m_Title   = "Defold Application";
    }

    HWindow WindowNew(void)
    {
        return dmPlatform::NewWindow();
    }

    WindowResult WindowOpen(HWindow window, const WindowCreateParams* params)
    {
        if (!params)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        return dmPlatform::OpenWindow(window, *params);
    }

    void WindowShow(HWindow window)
    {
        dmPlatform::ShowWindow(window);
    }

    void WindowIconify(HWindow window)
    {
        dmPlatform::IconifyWindow(window);
    }

    uint32_t WindowGetWidth(HWindow window)
    {
        return dmPlatform::GetWindowWidth(window);
    }

    uint32_t WindowGetHeight(HWindow window)
    {
        return dmPlatform::GetWindowHeight(window);
    }

    uint32_t WindowGetStateParam(HWindow window, WindowState state)
    {
        return dmPlatform::GetWindowStateParam(window, state);
    }

    float WindowGetDisplayScaleFactor(HWindow window)
    {
        return dmPlatform::GetDisplayScaleFactor(window);
    }

    void WindowSetSize(HWindow window, uint32_t width, uint32_t height)
    {
        dmPlatform::SetWindowSize(window, width, height);
    }

    void WindowClose(HWindow window)
    {
        dmPlatform::CloseWindow(window);
    }

    void WindowDelete(HWindow window)
    {
        dmPlatform::DeleteWindow(window);
    }

    void WindowPollEvents(HWindow window)
    {
        dmPlatform::PollEvents(window);
    }

    uintptr_t WindowGetProcAddress(HWindow window, const char* proc_name)
    {
        return dmPlatform::GetProcAddress(window, proc_name);
    }

    int32_t WindowGetKey(HWindow window, int32_t code)
    {
        return dmPlatform::GetKey(window, code);
    }

    int32_t WindowGetMouseButton(HWindow window, int32_t button)
    {
        return dmPlatform::GetMouseButton(window, button);
    }

    int32_t WindowGetMouseWheel(HWindow window)
    {
        return dmPlatform::GetMouseWheel(window);
    }

    void WindowGetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        dmPlatform::GetMousePosition(window, x, y);
    }

    uint32_t WindowGetTouchData(HWindow window, WindowTouchData* touch_data, uint32_t touch_data_count)
    {
        return dmPlatform::GetTouchData(window, touch_data, touch_data_count);
    }

    bool WindowGetAcceleration(HWindow window, float* x, float* y, float* z)
    {
        return dmPlatform::GetAcceleration(window, x, y, z);
    }

    bool WindowGetSafeArea(HWindow window, WindowSafeArea* out)
    {
        return dmPlatform::GetSafeArea(window, out);
    }

    const char* WindowGetJoystickDeviceName(HWindow window, uint32_t joystick_index)
    {
        return dmPlatform::GetJoystickDeviceName(window, joystick_index);
    }

    uint32_t WindowGetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity)
    {
        return dmPlatform::GetJoystickAxes(window, joystick_index, values, values_capacity);
    }

    uint32_t WindowGetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        return dmPlatform::GetJoystickHats(window, joystick_index, values, values_capacity);
    }

    uint32_t WindowGetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        return dmPlatform::GetJoystickButtons(window, joystick_index, values, values_capacity);
    }

    void WindowSetDeviceState(HWindow window, WindowDeviceState state, bool op1, bool op2)
    {
        dmPlatform::SetDeviceState(window, state, op1, op2);
    }

    bool WindowGetDeviceState(HWindow window, WindowDeviceState state, int32_t op1)
    {
        return dmPlatform::GetDeviceState(window, state, op1);
    }

    void WindowSetWindowTitle(HWindow window, const char* title)
    {
        dmPlatform::SetWindowTitle(window, title);
    }

    void WindowSetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        dmPlatform::SetWindowPosition(window, x, y);
    }

    void WindowSetSwapInterval(HWindow window, uint32_t swap_interval)
    {
        dmPlatform::SetSwapInterval(window, swap_interval);
    }

    void WindowSetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* user_data)
    {
        dmPlatform::SetKeyboardCharCallback(window, cb, user_data);
    }

    void WindowSetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* user_data)
    {
        dmPlatform::SetKeyboardMarkedTextCallback(window, cb, user_data);
    }

    void WindowSetKeyboardDeviceChangedCallback(HWindow window, FWindowDeviceChangedCallback cb, void* user_data)
    {
        dmPlatform::SetKeyboardDeviceChangedCallback(window, cb, user_data);
    }

    void WindowSetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* user_data)
    {
        dmPlatform::SetGamepadEventCallback(window, cb, user_data);
    }

    void WindowHide(HWindow window)
    {
        dmPlatform::HideWindow(window);
    }

    void WindowSwapBuffers(HWindow window)
    {
        dmPlatform::SwapBuffers(window);
    }

    void* WindowAcquireAuxContext(HWindow window)
    {
        return dmPlatform::AcquireAuxContext(window);
    }

    void WindowUnacquireAuxContext(HWindow window, void* aux_context)
    {
        dmPlatform::UnacquireAuxContext(window, aux_context);
    }

    int32_t WindowTriggerCloseCallback(HWindow window)
    {
        return dmPlatform::TriggerCloseCallback(window);
    }
}
