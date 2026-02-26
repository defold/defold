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

#include <dmsdk/platform/window.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*# device state enumeration
 * @enum
 * @name WindowDeviceState
 */
typedef enum WindowDeviceState
{
    WINDOW_DEVICE_STATE_CURSOR              = 1,
    WINDOW_DEVICE_STATE_CURSOR_LOCK         = 2,
    WINDOW_DEVICE_STATE_ACCELEROMETER       = 3,
    WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT    = 4,
    WINDOW_DEVICE_STATE_KEYBOARD_NUMBER_PAD = 5,
    WINDOW_DEVICE_STATE_KEYBOARD_EMAIL      = 6,
    WINDOW_DEVICE_STATE_KEYBOARD_PASSWORD   = 7,
    WINDOW_DEVICE_STATE_KEYBOARD_RESET      = 8,
    WINDOW_DEVICE_STATE_JOYSTICK_PRESENT    = 9,
    WINDOW_DEVICE_STATE_MAX
} WindowDeviceState;

/*# gamepad event enumeration
 * @enum
 * @name WindowGamepadEvent
 */
typedef enum WindowGamepadEvent
{
    WINDOW_GAMEPAD_EVENT_UNSUPPORTED  = 0,
    WINDOW_GAMEPAD_EVENT_CONNECTED    = 1,
    WINDOW_GAMEPAD_EVENT_DISCONNECTED = 2,
} WindowGamepadEvent;

/*# gamepad event callback
 * @typedef
 * @name FWindowGamepadEventCallback
 */
typedef void (*FWindowGamepadEventCallback)(void* user_data, int gamepad_index, WindowGamepadEvent evt);

/*# touch data
 * @struct
 * @name WindowTouchData
 */
typedef struct WindowTouchData
{
    int32_t m_TapCount;
    int32_t m_Phase;
    int32_t m_X;
    int32_t m_Y;
    int32_t m_DX;
    int32_t m_DY;
    int32_t m_Id;
} WindowTouchData;

/*# safe area
 * @struct
 * @name WindowSafeArea
 */
typedef struct WindowSafeArea
{
    int32_t   m_X;
    int32_t   m_Y;
    uint32_t  m_Width;
    uint32_t  m_Height;
    int32_t   m_InsetLeft;
    int32_t   m_InsetTop;
    int32_t   m_InsetRight;
    int32_t   m_InsetBottom;
} WindowSafeArea;

/*# get graphics proc address
 * @name WindowGetProcAddress
 * @param window [type:HWindow] window handle
 * @param proc_name [type:const char*] procedure name
 * @return address [type:uintptr_t] procedure address
 */
uintptr_t WindowGetProcAddress(HWindow window, const char* proc_name);

/*# get key state
 * @name WindowGetKey
 * @param window [type:HWindow] window handle
 * @param code [type:int32_t] key code
 * @return state [type:int32_t] key state
 */
int32_t WindowGetKey(HWindow window, int32_t code);

/*# get mouse button state
 * @name WindowGetMouseButton
 * @param window [type:HWindow] window handle
 * @param button [type:int32_t] mouse button
 * @return state [type:int32_t] button state
 */
int32_t WindowGetMouseButton(HWindow window, int32_t button);

/*# get mouse wheel delta
 * @name WindowGetMouseWheel
 * @param window [type:HWindow] window handle
 * @return delta [type:int32_t] wheel delta
 */
int32_t WindowGetMouseWheel(HWindow window);

/*# get mouse position
 * @name WindowGetMousePosition
 * @param window [type:HWindow] window handle
 * @param x [type:int32_t*] x coordinate
 * @param y [type:int32_t*] y coordinate
 */
void WindowGetMousePosition(HWindow window, int32_t* x, int32_t* y);

/*# get touch data
 * @name WindowGetTouchData
 * @param window [type:HWindow] window handle
 * @param touch_data [type:WindowTouchData*] touch data array
 * @param touch_data_count [type:uint32_t] array capacity
 * @return count [type:uint32_t] touch count
 */
uint32_t WindowGetTouchData(HWindow window, WindowTouchData* touch_data, uint32_t touch_data_count);

/*# get accelerometer data
 * @name WindowGetAcceleration
 * @param window [type:HWindow] window handle
 * @param x [type:float*] x axis
 * @param y [type:float*] y axis
 * @param z [type:float*] z axis
 * @return result [type:bool] true if available
 */
bool WindowGetAcceleration(HWindow window, float* x, float* y, float* z);

/*# get safe area
 * @name WindowGetSafeArea
 * @param window [type:HWindow] window handle
 * @param out [type:WindowSafeArea*] safe area
 * @return result [type:bool] true if available
 */
bool WindowGetSafeArea(HWindow window, WindowSafeArea* out);

/*# get joystick device name
 * @name WindowGetJoystickDeviceName
 * @param window [type:HWindow] window handle
 * @param joystick_index [type:uint32_t] joystick index
 * @return name [type:const char*] device name
 */
const char* WindowGetJoystickDeviceName(HWindow window, uint32_t joystick_index);

/*# get joystick axes
 * @name WindowGetJoystickAxes
 * @param window [type:HWindow] window handle
 * @param joystick_index [type:uint32_t] joystick index
 * @param values [type:float*] axis values
 * @param values_capacity [type:uint32_t] array capacity
 * @return count [type:uint32_t] axis count
 */
uint32_t WindowGetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity);

/*# get joystick hats
 * @name WindowGetJoystickHats
 * @param window [type:HWindow] window handle
 * @param joystick_index [type:uint32_t] joystick index
 * @param values [type:uint8_t*] hat values
 * @param values_capacity [type:uint32_t] array capacity
 * @return count [type:uint32_t] hat count
 */
uint32_t WindowGetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);

/*# get joystick buttons
 * @name WindowGetJoystickButtons
 * @param window [type:HWindow] window handle
 * @param joystick_index [type:uint32_t] joystick index
 * @param values [type:uint8_t*] button values
 * @param values_capacity [type:uint32_t] array capacity
 * @return count [type:uint32_t] button count
 */
uint32_t WindowGetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity);

/*# set device state
 * @name WindowSetDeviceState
 * @param window [type:HWindow] window handle
 * @param state [type:WindowDeviceState] state
 * @param op1 [type:bool] state parameter
 * @param op2 [type:bool] state parameter
 */
void WindowSetDeviceState(HWindow window, WindowDeviceState state, bool op1, bool op2);

/*# get device state
 * @name WindowGetDeviceState
 * @param window [type:HWindow] window handle
 * @param state [type:WindowDeviceState] state
 * @param op1 [type:int32_t] state parameter
 * @return result [type:bool] state value
 */
bool WindowGetDeviceState(HWindow window, WindowDeviceState state, int32_t op1);

/*# set window title
 * @name WindowSetWindowTitle
 * @param window [type:HWindow] window handle
 * @param title [type:const char*] window title
 */
void WindowSetWindowTitle(HWindow window, const char* title);

/*# set window position
 * @name WindowSetWindowPosition
 * @param window [type:HWindow] window handle
 * @param x [type:int32_t] x coordinate
 * @param y [type:int32_t] y coordinate
 */
void WindowSetWindowPosition(HWindow window, int32_t x, int32_t y);

/*# set swap interval
 * @name WindowSetSwapInterval
 * @param window [type:HWindow] window handle
 * @param swap_interval [type:uint32_t] swap interval
 */
void WindowSetSwapInterval(HWindow window, uint32_t swap_interval);

/*# set keyboard char callback
 * @name WindowSetKeyboardCharCallback
 * @param window [type:HWindow] window handle
 * @param cb [type:FWindowAddKeyboardCharCallback] callback
 * @param user_data [type:void*] callback context
 */
void WindowSetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* user_data);

/*# set keyboard marked text callback
 * @name WindowSetKeyboardMarkedTextCallback
 * @param window [type:HWindow] window handle
 * @param cb [type:FWindowSetMarkedTextCallback] callback
 * @param user_data [type:void*] callback context
 */
void WindowSetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* user_data);

/*# set keyboard device changed callback
 * @name WindowSetKeyboardDeviceChangedCallback
 * @param window [type:HWindow] window handle
 * @param cb [type:FWindowDeviceChangedCallback] callback
 * @param user_data [type:void*] callback context
 */
void WindowSetKeyboardDeviceChangedCallback(HWindow window, FWindowDeviceChangedCallback cb, void* user_data);

/*# set gamepad event callback
 * @name WindowSetGamepadEventCallback
 * @param window [type:HWindow] window handle
 * @param cb [type:FWindowGamepadEventCallback] callback
 * @param user_data [type:void*] callback context
 */
void WindowSetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* user_data);

/*# hide window
 * @name WindowHide
 * @param window [type:HWindow] window handle
 */
void WindowHide(HWindow window);

/*# swap buffers
 * @name WindowSwapBuffers
 * @param window [type:HWindow] window handle
 */
void WindowSwapBuffers(HWindow window);

/*# acquire aux context
 * @name WindowAcquireAuxContext
 * @param window [type:HWindow] window handle
 * @return context [type:void*] aux context
 */
void* WindowAcquireAuxContext(HWindow window);

/*# unacquire aux context
 * @name WindowUnacquireAuxContext
 * @param window [type:HWindow] window handle
 * @param aux_context [type:void*] aux context
 */
void WindowUnacquireAuxContext(HWindow window, void* aux_context);

/*# trigger close callback
 * @name WindowTriggerCloseCallback
 * @param window [type:HWindow] window handle
 * @return result [type:int32_t] callback result
 */
int32_t WindowTriggerCloseCallback(HWindow window);

#ifdef __cplusplus
}
#endif

#endif // DM_PLATFORM_WINDOW_H
