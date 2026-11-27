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

#ifndef DM_PLATFORM_WINDOW_MOBILE_WEB_BACKEND_H
#define DM_PLATFORM_WINDOW_MOBILE_WEB_BACKEND_H

#include "joystick_guid.h"
#include "window_mobile_web_constants.h"
#include "window_touch.h"
#include "dmsdk/platform/window.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    NATIVE_KEYBOARD_DEFAULT, NATIVE_KEYBOARD_NUMBER_PAD, NATIVE_KEYBOARD_EMAIL, NATIVE_KEYBOARD_PASSWORD
};
enum
{
    NATIVE_NO_API, NATIVE_OPENGL_API = 0x30001
};

typedef void (*Nativecharfun)(int codepoint, int action);
typedef void (*Nativemarkedtextfun)(char* text);
typedef void (*Nativegamepadfun)(int slot, int connected);
typedef void (*Nativedevicechangedfun)(int status);
typedef void (*Nativewindowsizefun)(int width, int height);
typedef int  (*Nativewindowclosefun)(void);
typedef void (*Nativewindowfocusfun)(int focused);
typedef void (*Nativewindowiconifyfun)(int iconified);

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
uint32_t dmNativeGetTouch(WindowTouchData* output, uint32_t capacity);

int dmNativeGetAcceleration(float* x, float* y, float* z);

int  dmNativeGetJoystickParam(int joy, int param);
int  dmNativeGetJoystickPos(int joy, float* pos, int numaxes);
int  dmNativeGetJoystickButtons(int joy, unsigned char* buttons, int numbuttons);
int  dmNativeGetJoystickHats(int joy, unsigned char* hats, int numhats);
int  dmNativeGetJoystickDeviceId(int joy, char** device_id);
int  dmNativeGetJoystickDeviceGuid(int joy, char** device_guid);

void* dmNativeGetProcAddress(const char* procname);

void  dmNativeSetViewType(int view_type);
void  dmNativeSetWindowBackgroundColor(unsigned int color);
float dmNativeGetDisplayScaleFactor(void);
float dmNativeGetDisplayDensity(void);
int   dmNativeGetMouseLocked(void);

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
