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

#ifndef DM_PLATFORM_WINDOW_CPP_H
#define DM_PLATFORM_WINDOW_CPP_H

#include "window.h"

namespace dmPlatform
{
    HWindow        NewWindow();
    void           DeleteWindow(HWindow window);
    WindowResult   OpenWindow(HWindow window, const WindowCreateParams& params);
    void           CloseWindow(HWindow window);

    void           ShowWindow(HWindow window);
    void           IconifyWindow(HWindow window);
    void           PollEvents(HWindow window);

    uint32_t       GetWindowWidth(HWindow window);
    uint32_t       GetWindowHeight(HWindow window);
    uint32_t       GetWindowStateParam(HWindow window, WindowState state);
    float          GetDisplayScaleFactor(HWindow window);
    uintptr_t      GetProcAddress(HWindow window, const char* proc_name);

    int32_t        GetKey(HWindow window, int32_t code);
    int32_t        GetMouseButton(HWindow window, int32_t button);
    int32_t        GetMouseWheel(HWindow window);
    void           GetMousePosition(HWindow window, int32_t* x, int32_t* y);
    uint32_t       GetTouchData(HWindow window, WindowTouchData* touch_data, uint32_t touch_data_count);
    bool           GetAcceleration(HWindow window, float* x, float* y, float* z);
    bool           GetSafeArea(HWindow window, WindowSafeArea* out);

    const char*    GetJoystickDeviceName(HWindow window, uint32_t joystick_index);
    uint32_t       GetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity);
    uint32_t       GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);
    uint32_t       GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);

    void           SetDeviceState(HWindow window, WindowDeviceState state, bool op1);
    void           SetDeviceState(HWindow window, WindowDeviceState state, bool op1, bool op2);
    bool           GetDeviceState(HWindow window, WindowDeviceState state);
    bool           GetDeviceState(HWindow window, WindowDeviceState state, int32_t op1);

    void           SetWindowTitle(HWindow window, const char* title);
    void           SetWindowSize(HWindow window, uint32_t width, uint32_t height);
    void           SetWindowPosition(HWindow window, int32_t x, int32_t y);
    void           SetSwapInterval(HWindow window, uint32_t swap_interval);
    void           SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* user_data);
    void           SetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* user_data);
    void           SetKeyboardDeviceChangedCallback(HWindow window, FWindowDeviceChangedCallback cb, void* user_data);
    void           SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* user_data);

    void           HideWindow(HWindow window);
    void           SwapBuffers(HWindow window);
    void*          AcquireAuxContext(HWindow window);
    void           UnacquireAuxContext(HWindow window, void* aux_context);

    int32_t        TriggerCloseCallback(HWindow window);
}

#endif // DM_PLATFORM_WINDOW_CPP_H
