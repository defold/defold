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

#ifndef DM_WINDOW_MOBILE_PRIVATE_H
#define DM_WINDOW_MOBILE_PRIVATE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "window_mobile_web_constants.h"
#include "window_touch.h"
#if defined(ANDROID)
#include "android/window_android.h"
#elif defined(DM_PLATFORM_IOS)
#include "ios/window_ios.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    NATIVE_NO_API, NATIVE_OPENGL_API = 0x30001
};
enum
{
    NATIVE_KEYBOARD_DEFAULT, NATIVE_KEYBOARD_NUMBER_PAD, NATIVE_KEYBOARD_EMAIL, NATIVE_KEYBOARD_PASSWORD
};

typedef struct
{
    FWindowAddKeyboardCharCallback m_CharCallback;
    void*                          m_CharUserData;
    FWindowSetMarkedTextCallback   m_MarkedCallback;
    void*                          m_MarkedUserData;
    FWindowGamepadEventCallback    m_GamepadCallback;
    void*                          m_GamepadUserData;
} MobileInputCallbacks;

typedef struct
{
    NativeTouch Touch[NATIVE_MAX_TOUCH];
    uint8_t     keys[NATIVE_KEY_LAST + 1];
    uint8_t     heldButtons;
    uint8_t     releasedButtons;
    int         MousePosX, MousePosY;
    int         MouseLeftButtonFromTouch, MousePositionFromTouch;
    float       AccX, AccY, AccZ;
    void*       MouseEmulationTouch;
} NativeInput;

typedef struct
{
    int                  opened, active, iconified;
    int                  width, height;
    int                  clientAPI;
    int                  cursorHidden;
    void (*windowSizeCallback)(int width, int height);
    FWindowFocusCallback  windowFocusCallback;
    void*                 windowFocusUserData;
    MobileInputCallbacks* inputCallbacks;
#if defined(DM_PLATFORM_IOS)
    id  window, viewController, view;
    id  context, aux_context;
    int portrait, frameBuffer;
#endif
} Nativewin;

extern Nativewin dmNativeWin;
extern NativeInput dmNativeInput;
extern int dmNativeInitialized;

int  dmNativeInit(void);
void dmNativeTerminate(void);
void dmNativeCloseWindow(void);
int  dmNativeGetKey(int key);
int  dmNativeGetMouseButton(int button);
void dmNativeShowKeyboard(int show, int type, int auto_close);
void dmNativeResetKeyboard(void);
void dmNativeAccelerometerEnable(void);
void dmNativeClearInput(void);
void dmNativeInputKey(int key, int action);
void dmNativeInputChar(int character, int action);
void dmNativeSetMarkedText(char* text);
void dmNativeInputMouseClick(int button, int action);

int   dmNativeHasRequiredGLESVersion(void);
int   dmNativeOSInit(void);
int   dmNativeOSTerminate(void);
int   dmNativeOSOpenWindow(int width, int height, int client_api);
void  dmNativeOSCloseWindow(void);
int   dmNativeOSGetDefaultFramebuffer(void);
int   dmNativeOSGetWindowRefreshRate(void);
void  dmNativeOSSetWindowSize(int width, int height);
void  dmNativeOSIconifyWindow(void);
void  dmNativeOSSwapBuffers(void);
void  dmNativeOSSwapInterval(int interval);
void  dmNativeOSPollEvents(void);
int   dmNativeOSQueryAuxContext(void);
void* dmNativeOSAcquireAuxContext(void);
void  dmNativeOSUnacquireAuxContext(void* context);
void  dmNativeOSSetWindowBackgroundColor(unsigned int color);
int   dmNativeOSGetAcceleration(float* x, float* y, float* z);

#ifdef __cplusplus
}
#endif

#endif
