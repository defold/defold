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

#ifndef DM_PLATFORM_WINDOW_H
#define DM_PLATFORM_WINDOW_H

#include <stdint.h>
#include <string.h>

namespace dmPlatform
{
    struct dmWindow;
    typedef dmWindow* HWindow;

    typedef void (*WindowResizeCallback)(void* user_data, uint32_t width, uint32_t height);
    typedef void (*WindowFocusCallback)(void* user_data, uint32_t focus);
    typedef void (*WindowIconifyCallback)(void* user_data, uint32_t iconified);
    typedef bool (*WindowCloseCallback)(void* user_data);

    typedef void (*WindowAddKeyboardCharCallback)(void* user_data, int chr);
    typedef void (*WindowSetMarkedTextCallback)(void* user_data, char* text);
    typedef void (*WindowDeviceChangedCallback)(void* user_data, int status);

    enum PlatformResult
    {
        PLATFORM_RESULT_OK                    = 0,
        PLATFORM_RESULT_WINDOW_OPEN_ERROR     = -1,
        PLATFORM_RESULT_WINDOW_ALREADY_OPENED = -2,
    };

    enum PlatformGraphicsApi
    {
        PLATFORM_GRAPHICS_API_NULL     = 0,
        PLATFORM_GRAPHICS_API_OPENGL   = 1,
        PLATFORM_GRAPHICS_API_OPENGLES = 2,
        PLATFORM_GRAPHICS_API_VULKAN   = 3,
        PLATFORM_GRAPHICS_API_VENDOR   = 4,
        PLATFORM_GRAPHICS_API_WEBGPU   = 5,
        PLATFORM_GRAPHICS_API_DIRECTX  = 6,
    };

    enum DeviceState
    {
        DEVICE_STATE_CURSOR              = 1,
        DEVICE_STATE_CURSOR_LOCK         = 2,
        DEVICE_STATE_ACCELEROMETER       = 3,
        DEVICE_STATE_KEYBOARD_DEFAULT    = 4,
        DEVICE_STATE_KEYBOARD_NUMBER_PAD = 5,
        DEVICE_STATE_KEYBOARD_EMAIL      = 6,
        DEVICE_STATE_KEYBOARD_PASSWORD   = 7,
        DEVICE_STATE_KEYBOARD_RESET      = 8,
        DEVICE_STATE_JOYSTICK_PRESENT    = 9,
        DEVICE_STATE_MAX // Used to create arrays of correct size (private repo)
    };

    enum GamepadEvent
    {
        GAMEPAD_EVENT_UNSUPPORTED  = 0,
        GAMEPAD_EVENT_CONNECTED    = 1,
        GAMEPAD_EVENT_DISCONNECTED = 2,
    };

    enum WindowState
    {
        WINDOW_STATE_OPENED             = 1,
        WINDOW_STATE_ACTIVE             = 2,
        WINDOW_STATE_ICONIFIED          = 3,
        WINDOW_STATE_ACCELERATED        = 4,
        WINDOW_STATE_RED_BITS           = 5,
        WINDOW_STATE_GREEN_BITS         = 6,
        WINDOW_STATE_BLUE_BITS          = 7,
        WINDOW_STATE_ALPHA_BITS         = 8,
        WINDOW_STATE_DEPTH_BITS         = 9,
        WINDOW_STATE_STENCIL_BITS       = 10,
        WINDOW_STATE_REFRESH_RATE       = 11,
        WINDOW_STATE_ACCUM_RED_BITS     = 12,
        WINDOW_STATE_ACCUM_GREEN_BITS   = 13,
        WINDOW_STATE_ACCUM_BLUE_BITS    = 14,
        WINDOW_STATE_ACCUM_ALPHA_BITS   = 15,
        WINDOW_STATE_AUX_BUFFERS        = 16,
        WINDOW_STATE_STEREO             = 17,
        WINDOW_STATE_WINDOW_NO_RESIZE   = 18,
        WINDOW_STATE_FSAA_SAMPLES       = 19,
        WINDOW_STATE_SAMPLE_COUNT       = 20,
        WINDOW_STATE_HIGH_DPI           = 21,
        WINDOW_STATE_AUX_CONTEXT        = 22,
    };

    typedef void (*WindowGamepadEventCallback)(void* user_data, int gamepad_index, GamepadEvent evt);

    struct WindowParams
    {
        WindowParams()
        {
            memset(this, 0, sizeof(*this));
            m_Width   = 640;
            m_Height  = 480;
            m_Samples = 1;
            m_Title   = "Defold Application";
        }

        PlatformGraphicsApi     m_GraphicsApi;
        /// Window resize callback
        WindowResizeCallback    m_ResizeCallback;
        /// User data supplied to the callback function
        void*                   m_ResizeCallbackUserData;
        /// Window close callback
        WindowCloseCallback     m_CloseCallback;
        /// User data supplied to the callback function
        void*                   m_CloseCallbackUserData;
        /// Window focus callback
        WindowFocusCallback     m_FocusCallback;
        /// User data supplied to the callback function
        void*                   m_FocusCallbackUserData;
        /// Window iconify callback
        WindowIconifyCallback   m_IconifyCallback;
        /// User data supplied to the callback function
        void*                   m_IconifyCallbackUserData;
        /// Window width, 640 by default
        uint32_t                m_Width;
        /// Window height, 480 by default
        uint32_t                m_Height;
        /// Number of samples (for multi-sampling), 1 by default
        uint32_t                m_Samples;
        /// Window title, "Dynamo App" by default
        const char*             m_Title;
        /// If the window should cover the full screen or not, false by default
        bool                    m_Fullscreen;
        /// Log info about the graphics device being used, false by default
        bool                    m_PrintDeviceInfo;
        ///
        bool                    m_HighDPI;
        // Window background color, RGB 0x00BBGGRR
        uint32_t                m_BackgroundColor;
        uint8_t                 m_ContextAlphabits;
        // OpenGL specific settings
        uint8_t                 m_OpenGLVersionHint        : 7; // I.e: 33, 40-46, 0 (use highest available)
        uint8_t                 m_OpenGLUseCoreProfileHint : 1;
    };

    struct TouchData
    {
        int32_t m_TapCount;
        int32_t m_Phase;
        int32_t m_X;
        int32_t m_Y;
        int32_t m_DX;
        int32_t m_DY;
        int32_t m_Id;
    };

    HWindow        NewWindow();
    void           DeleteWindow(HWindow window);
    PlatformResult OpenWindow(HWindow window, const WindowParams& params);
    void           CloseWindow(HWindow window);

    void           AddOnResizeCallback(HWindow window, WindowResizeCallback cb, void* user_data);

    uint32_t       GetWindowWidth(HWindow window);
    uint32_t       GetWindowHeight(HWindow window);
    uint32_t       GetWindowStateParam(HWindow window, WindowState state);
    float          GetDisplayScaleFactor(HWindow window);
    uintptr_t      GetProcAddress(HWindow window, const char* proc_name);

    int32_t        GetKey(HWindow window, int32_t code);
    int32_t        GetMouseButton(HWindow window, int32_t button);
    int32_t        GetMouseWheel(HWindow window);
    void           GetMousePosition(HWindow window, int32_t* x, int32_t* y);
    uint32_t       GetTouchData(HWindow window, TouchData* touch_data, uint32_t touch_data_count);
    bool           GetAcceleration(HWindow window, float* x, float* y, float* z);

    const char*    GetJoystickDeviceName(HWindow window, uint32_t joystick_index);
    uint32_t       GetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity);
    uint32_t       GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);
    uint32_t       GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);

    void           SetDeviceState(HWindow window, DeviceState state, bool op1);
    void           SetDeviceState(HWindow window, DeviceState state, bool op1, bool op2);
    bool           GetDeviceState(HWindow window, DeviceState state);
    bool           GetDeviceState(HWindow window, DeviceState state, int32_t op1);

    void           SetWindowTitle(HWindow window, const char* title);
    void           SetWindowSize(HWindow window, uint32_t width, uint32_t height);
    void           SetWindowPosition(HWindow window, int32_t x, int32_t y);
    void           SetSwapInterval(HWindow window, uint32_t swap_interval);
    void           SetKeyboardCharCallback(HWindow window, WindowAddKeyboardCharCallback cb, void* user_data);
    void           SetKeyboardMarkedTextCallback(HWindow window, WindowSetMarkedTextCallback cb, void* user_data);
    void           SetKeyboardDeviceChangedCallback(HWindow window, WindowDeviceChangedCallback cb, void* user_data);
    void           SetGamepadEventCallback(HWindow window, WindowGamepadEventCallback cb, void* user_data);

    void           ShowWindow(HWindow window);
    void           IconifyWindow(HWindow window);
    void           PollEvents(HWindow window);
    void           SwapBuffers(HWindow window);

    void*          AcquireAuxContext(HWindow window);
    void           UnacquireAuxContext(HWindow window, void* aux_context);

    // For tests
    int32_t TriggerCloseCallback(HWindow window);
};

#endif // DM_PLATFORM_WINDOW_H
