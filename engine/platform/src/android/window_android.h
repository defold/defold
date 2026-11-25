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

#ifndef DM_WINDOW_ANDROID_H
#define DM_WINDOW_ANDROID_H

#include <pthread.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include "window_mobile_web_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

EGLContext          dmNativeGetAndroidEGLContext(void);
EGLSurface          dmNativeGetAndroidEGLSurface(void);
JavaVM*             dmNativeGetAndroidJavaVM(void);
jobject             dmNativeGetAndroidActivity(void);
struct android_app* dmNativeGetAndroidApp(void);
ANativeWindow*      dmNativeAcquireAndroidWindow(void);
ANativeWindow*      dmNativeWaitForAndroidWindow(void);
int                 dmNativeAndroidIsWindowCurrent(ANativeWindow* window);
void                dmNativeReleaseAndroidWindow(ANativeWindow* window);

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

typedef struct Nativewin_android_struct Nativewin_android;

struct Nativewin_android_struct
{
    EGLDisplay          display;
    EGLContext          context;
    EGLContext          aux_context;
    EGLConfig           config;
    EGLSurface          surface;
    EGLSurface          aux_surface;
    struct android_app* app;
    ANativeWindow*      native_window;
    // pipe used to go from java thread to native (JNI)
    int                 m_Pipefd[2];
    uint32_t            m_RenderLock; // Set if we are between "frame begin" and "swap buffers"
    uint8_t             egl_bad_alloc_retry_count;
    uint8_t             should_recreate_surface : 1;
    uint8_t : 7;
};

extern Nativewin_android dmNativeWinAndroid;

#define DEVICE_NAME_LENGTH 64
#define NATIVE_ANDROID_GAMEPAD_NUMBUTTONS 36
#define NATIVE_ANDROID_GAMEPAD_NUMAXIS 8
#define NATIVE_ANDROID_GAMEPAD_MAX_AXES 32

typedef struct
{
    int           State;
    int           DeviceId;
    char          DeviceName[DEVICE_NAME_LENGTH];
    char          DeviceGuid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH + 1];
    int           NumAxes;
    int           NumButtons;
    // Legacy axes keep their fixed slots; device-only axes use slots 8 + device axis index.
    float         Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + NATIVE_ANDROID_GAMEPAD_MAX_AXES];
    // Legacy key slots plus separate Back, Menu and Record aliases.
    unsigned char Button[NATIVE_ANDROID_GAMEPAD_NUMBUTTONS + 3];
    unsigned char Hats;
    int           LayoutLegacy;
    // Keep mapping defaults separate from observed axis values.
    uint64_t      AxisEventMask;
    uint64_t      AxisInitialMask;
    int           AxisIds[NATIVE_ANDROID_GAMEPAD_MAX_AXES];
    float         AxisMin[NATIVE_ANDROID_GAMEPAD_MAX_AXES];
    float         AxisRange[NATIVE_ANDROID_GAMEPAD_MAX_AXES];
} NativeAndroidJoystick;
extern NativeAndroidJoystick dmNativeJoy[NATIVE_JOYSTICK_LAST + 1];
void dmNativeTerminateJoysticks(void);

#ifdef __cplusplus
}
#endif

#endif
