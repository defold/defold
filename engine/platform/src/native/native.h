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

#ifndef DM_PLATFORM_NATIVE_H
#define DM_PLATFORM_NATIVE_H

#include "native_touch.h"
#include "../dmsdk/platform/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// Values shared by mobile input, browser events and the platform key constants.
enum NativeKey
{
    NATIVE_KEY_SPACE = ' ',
    NATIVE_KEY_ESC   = 257,
    NATIVE_KEY_F1, NATIVE_KEY_F2, NATIVE_KEY_F3, NATIVE_KEY_F4,
    NATIVE_KEY_F5, NATIVE_KEY_F6, NATIVE_KEY_F7, NATIVE_KEY_F8,
    NATIVE_KEY_F9, NATIVE_KEY_F10, NATIVE_KEY_F11, NATIVE_KEY_F12,
    NATIVE_KEY_UP = 283, NATIVE_KEY_DOWN, NATIVE_KEY_LEFT, NATIVE_KEY_RIGHT,
    NATIVE_KEY_LSHIFT, NATIVE_KEY_RSHIFT, NATIVE_KEY_LCTRL, NATIVE_KEY_RCTRL,
    NATIVE_KEY_LALT, NATIVE_KEY_RALT, NATIVE_KEY_TAB, NATIVE_KEY_ENTER,
    NATIVE_KEY_BACKSPACE, NATIVE_KEY_INSERT, NATIVE_KEY_DEL, NATIVE_KEY_PAGEUP,
    NATIVE_KEY_PAGEDOWN, NATIVE_KEY_HOME, NATIVE_KEY_END,
    NATIVE_KEY_KP_0, NATIVE_KEY_KP_1, NATIVE_KEY_KP_2, NATIVE_KEY_KP_3,
    NATIVE_KEY_KP_4, NATIVE_KEY_KP_5, NATIVE_KEY_KP_6, NATIVE_KEY_KP_7,
    NATIVE_KEY_KP_8, NATIVE_KEY_KP_9, NATIVE_KEY_KP_DIVIDE, NATIVE_KEY_KP_MULTIPLY,
    NATIVE_KEY_KP_SUBTRACT, NATIVE_KEY_KP_ADD, NATIVE_KEY_KP_DECIMAL, NATIVE_KEY_KP_EQUAL,
    NATIVE_KEY_KP_ENTER, NATIVE_KEY_KP_NUM_LOCK, NATIVE_KEY_CAPS_LOCK, NATIVE_KEY_SCROLL_LOCK,
    NATIVE_KEY_PAUSE, NATIVE_KEY_LSUPER, NATIVE_KEY_RSUPER, NATIVE_KEY_MENU, NATIVE_KEY_BACK,
    NATIVE_KEY_LAST = NATIVE_KEY_BACK
};

enum
{
    NATIVE_RELEASE, NATIVE_PRESS
};
enum
{
    NATIVE_KEYBOARD_DEFAULT, NATIVE_KEYBOARD_NUMBER_PAD, NATIVE_KEYBOARD_EMAIL, NATIVE_KEYBOARD_PASSWORD
};
enum
{
    NATIVE_MOUSE_BUTTON_1, NATIVE_MOUSE_BUTTON_2, NATIVE_MOUSE_BUTTON_3, NATIVE_MOUSE_BUTTON_4,
    NATIVE_MOUSE_BUTTON_5, NATIVE_MOUSE_BUTTON_6, NATIVE_MOUSE_BUTTON_7, NATIVE_MOUSE_BUTTON_8,
    NATIVE_MOUSE_BUTTON_LAST   = NATIVE_MOUSE_BUTTON_8,
    NATIVE_MOUSE_BUTTON_LEFT   = NATIVE_MOUSE_BUTTON_1,
    NATIVE_MOUSE_BUTTON_RIGHT  = NATIVE_MOUSE_BUTTON_2,
    NATIVE_MOUSE_BUTTON_MIDDLE = NATIVE_MOUSE_BUTTON_3
};
enum
{
    NATIVE_JOYSTICK_1, NATIVE_JOYSTICK_2, NATIVE_JOYSTICK_3, NATIVE_JOYSTICK_4,
    NATIVE_JOYSTICK_5, NATIVE_JOYSTICK_6, NATIVE_JOYSTICK_7, NATIVE_JOYSTICK_8,
    NATIVE_JOYSTICK_9, NATIVE_JOYSTICK_10, NATIVE_JOYSTICK_11, NATIVE_JOYSTICK_12,
    NATIVE_JOYSTICK_13, NATIVE_JOYSTICK_14, NATIVE_JOYSTICK_15, NATIVE_JOYSTICK_16,
    NATIVE_JOYSTICK_LAST = NATIVE_JOYSTICK_16
};
enum
{
    NATIVE_HAT_CENTERED = 0, NATIVE_HAT_UP = 1, NATIVE_HAT_RIGHT = 2, NATIVE_HAT_DOWN = 4, NATIVE_HAT_LEFT = 8
};
// Also used by the browser Gamepad API bridge.
#define NATIVE_PRESENT 0x50001
#define NATIVE_AXES    0x50002
#define NATIVE_BUTTONS 0x50003
#define NATIVE_HATS    0x50004
enum
{
    NATIVE_NO_API, NATIVE_OPENGL_API = 0x30001
};
enum
{
    NATIVE_JOYSTICK_DEVICE_GUID_LENGTH = 32
};

typedef void (*Nativecharfun)(int codepoint, int action);
typedef void (*Nativemarkedtextfun)(char* text);
typedef void (*Nativegamepadfun)(int slot, int connected);
typedef void (*Nativedevicechangedfun)(int status);
typedef void (*Nativewindowsizefun)(int width, int height);
typedef int  (*Nativewindowclosefun)(void);
typedef void (*Nativewindowfocusfun)(int focused);
typedef void (*Nativewindowiconifyfun)(int iconified);

#if defined(ANDROID)
struct android_app;
struct AInputEvent;
#endif

int  dmNativeInit(void);
void dmNativeTerminate(void);

int  dmNativeOpenWindow(const WindowCreateParams* params);
void dmNativeSetCursorVisible(int visible);
void dmNativeCloseWindow(void);
int  dmNativeGetDefaultFramebuffer(void);
void dmNativeSetWindowTitle(const char* title);
void dmNativeGetWindowSize(int* width, int* height);
void dmNativeSetWindowSize(int width, int height);
void dmNativeSetWindowPos(int x, int y);
void dmNativeIconifyWindow(void);
void dmNativeSwapBuffers(void);
void dmNativeSwapInterval(int interval);
int  dmNativeGetWindowParam(WindowState param);
void dmNativeSetWindowSizeCallback(Nativewindowsizefun cbfun);
void dmNativeSetWindowCloseCallback(Nativewindowclosefun cbfun);
int  dmNativeGetWindowRefreshRate(void);

void dmNativePollEvents(void);
int  dmNativeGetKey(int key);
int  dmNativeGetMouseButton(int button);
int  dmNativeIsMouseLeftButtonFromTouch(void);
int  dmNativeIsMousePositionFromTouch(void);
void dmNativeGetMousePos(int* xpos, int* ypos);
int  dmNativeGetMouseWheel(void);
int  dmNativeSetCharCallback(Nativecharfun cbfun);
int  dmNativeSetMarkedTextCallback(Nativemarkedtextfun cbfun);
void dmNativeShowKeyboard(int show, int type, int auto_close);
void dmNativeResetKeyboard(void);
int  dmNativeGetTouch(NativeTouch* touch, int count, int* out_count);

int dmNativeGetAcceleration(float* x, float* y, float* z);

void dmNativeCreateJoystickDeviceGuid(unsigned short bus, unsigned short vendor, unsigned short product, unsigned short version, const char* vendor_name, const char* product_name, unsigned char driver_signature, unsigned char driver_data, char guid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH + 1]);
int  dmNativeGetJoystickParam(int joy, int param);
int  dmNativeGetJoystickPos(int joy, float* pos, int numaxes);
int  dmNativeGetJoystickButtons(int joy, unsigned char* buttons, int numbuttons);
int  dmNativeGetJoystickHats(int joy, unsigned char* hats, int numhats);
int  dmNativeGetJoystickDeviceId(int joy, char** device_id);
int  dmNativeGetJoystickDeviceGuid(int joy, char** device_guid);

void* dmNativeGetProcAddress(const char* procname);

void  dmNativeRegisterUIApplicationDelegate(void* delegate);
void  dmNativeUnregisterUIApplicationDelegate(void* delegate);
void  dmNativeRegisterUISceneDelegate(void* delegate);
void  dmNativeUnregisterUISceneDelegate(void* delegate);
void  dmNativeSetViewType(int view_type);
void  dmNativeSetWindowBackgroundColor(unsigned int color);
float dmNativeGetDisplayScaleFactor(void);
float dmNativeGetDisplayDensity(void);
int   dmNativeGetMouseLocked(void);

#if defined(ANDROID)
void    dmNativeAndroidBeginFrame(void);
void    dmNativeAndroidHandleCommand(struct android_app* app, int32_t cmd);
int32_t dmNativeAndroidHandleInput(struct android_app* app, struct AInputEvent* event);
int32_t dmNativeAndroidWindowOpened(void);
void    dmNativeAndroidPollEvents(void);
void    dmNativeAndroidFlushEvents(void);
int32_t dmNativeAndroidVerifySurface(void);

// Activity control
typedef void (*dmNativeactivityresultfun)(void* env, void* activity, int request_code, int result_code, void* result);
void dmNativeAndroidRegisterOnActivityResultListener(dmNativeactivityresultfun fun);
void dmNativeAndroidUnregisterOnActivityResultListener(dmNativeactivityresultfun fun);

// onCreate listeners
typedef void (*dmNativeoncreatefun)(void* env, void* activity);
void dmNativeAndroidRegisterOnCreateListener(dmNativeoncreatefun fun);
void dmNativeAndroidUnregisterOnCreateListener(dmNativeoncreatefun fun);
#endif

// Accelerometer control
void dmNativeAccelerometerEnable(void);

// context control
int   dmNativeQueryAuxContext(void);
void* dmNativeAcquireAuxContext(void);
void  dmNativeUnacquireAuxContext(void* context);

// Window and device callbacks
void dmNativeSetWindowFocusCallback(Nativewindowfocusfun cbfun);
void dmNativeSetWindowIconifyCallback(Nativewindowiconifyfun cbfun);
int  dmNativeSetGamepadCallback(Nativegamepadfun cbfun);
int  dmNativeSetDeviceChangedCallback(Nativedevicechangedfun cbfun);

#ifdef __cplusplus
}
#endif
#endif
