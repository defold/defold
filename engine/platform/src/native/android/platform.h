//========================================================================
// GLFW - An OpenGL framework
// Platform:    X11/GLX
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================


// Modified for Defold: private mobile/web backend, without the GLFW API.
#ifndef _platform_h_
#define _platform_h_


// This is the X11 version of Native
#define Native_ANDROID

// Include files
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <android_native_app_glue.h>
#include "native.h"
#include "native_handles.h"

// Do we have pthread support?
#ifdef Native_HAS_PTHREAD
 #include <pthread.h>
 #include <sched.h>
#endif

#define dmNative_numprocessors(n) n=1

// Pointer length integer
// One day, this will most likely move into native.h
typedef intptr_t Nativeintptr;

#ifndef GL_VERSION_3_0
typedef const GLubyte * (APIENTRY *PFNGLGETSTRINGIPROC) (GLenum, GLuint);
#endif /*GL_VERSION_3_0*/

//========================================================================
// Global variables (Native internals)
//========================================================================

//------------------------------------------------------------------------
// Window structure
//------------------------------------------------------------------------
typedef struct Nativewin_struct Nativewin;

struct Nativewin_struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // User callback functions
    Nativewindowsizefun    windowSizeCallback;
    Nativewindowclosefun   windowCloseCallback;
    Nativewindowrefreshfun windowRefreshCallback;
    Nativewindowfocusfun   windowFocusCallback;
    Nativewindowiconifyfun windowIconifyCallback;
    Nativemousebuttonfun   mouseButtonCallback;
    Nativemouseposfun      mousePosCallback;
    Nativemousewheelfun    mouseWheelCallback;
    Nativekeyfun           keyCallback;
    Nativecharfun          charCallback;
    Nativemarkedtextfun    markedTextCallback;
    Nativegamepadfun       gamepadCallback;

    // User selected window settings
    int       fullscreen;      // Fullscreen flag
    int       mouseLock;       // Mouse-lock flag
    int       autoPollEvents;  // Auto polling flag
    int       sysKeysDisabled; // System keys disabled flag
    int       windowNoResize;  // Resize- and maximize gadgets disabled flag
    int       refreshRate;     // Vertical monitor refresh rate

    // Window status & parameters
    int       opened;          // Flag telling if window is opened or not
    int       active;          // Application active flag
    int       iconified;       // Window iconified flag
    int       width, height;   // Window width and heigth
    int       accelerated;     // GL_TRUE if window is HW accelerated

    // Framebuffer attributes
    int       redBits;
    int       greenBits;
    int       blueBits;
    int       alphaBits;
    int       depthBits;
    int       stencilBits;
    int       accumRedBits;
    int       accumGreenBits;
    int       accumBlueBits;
    int       accumAlphaBits;
    int       auxBuffers;
    int       stereo;
    int       samples;

    // OpenGL extensions and context attributes
    int       has_GL_SGIS_generate_mipmap;
    int       has_GL_ARB_texture_non_power_of_two;
    int       glMajor, glMinor, glRevision;
    int       glForward, glDebug, glProfile;
    int       highDPI;
    int       clientAPI;

    PFNGLGETSTRINGIPROC GetStringi;
};

NativeGLOBAL Nativewin dmNativeWin;

// ========= PLATFORM SPECIFIC PART ======================================

typedef struct Nativewin_android_struct Nativewin_android;

struct Nativewin_android_struct {
    EGLDisplay display;
    EGLContext context;
    EGLContext aux_context;
    EGLConfig config;
    EGLSurface surface;
    EGLSurface aux_surface;
    struct android_app* app;
    ANativeWindow* native_window;
    // pipe used to go from java thread to native (JNI)
    int m_Pipefd[2];
    uint32_t m_RenderLock; // Set if we are between "frame begin" and "swap buffers"
    uint8_t egl_bad_alloc_retry_count;
    uint8_t should_recreate_surface:1;
    uint8_t :7;
};

NativeGLOBAL Nativewin_android dmNativeWinAndroid;


//------------------------------------------------------------------------
// User input status (most of this should go in Nativewin)
//------------------------------------------------------------------------
NativeGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Mouse status
    int  MousePosX, MousePosY;
    int  WheelPos;
    char MouseButton[ NATIVE_MOUSE_BUTTON_LAST+1 ];

    // Keyboard status
    char Key[ NATIVE_KEY_LAST+1 ];
    int  LastChar;

    // User selected settings
    int  StickyKeys;
    int  StickyMouseButtons;
    int  KeyRepeat;

    NativeTouch Touch[NATIVE_MAX_TOUCH];

// ========= PLATFORM SPECIFIC PART ======================================

    // Platform specific internal variables
    int  MouseMoved, CursorPosX, CursorPosY;
    float AccX, AccY, AccZ;

} dmNativeInput;


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Joystick information & state
//------------------------------------------------------------------------
#define DEVICE_NAME_LENGTH 64
#define DEVICE_GUID_LENGTH 32 // DEFOLD
#define NATIVE_ANDROID_GAMEPAD_NUMBUTTONS 36
#define NATIVE_ANDROID_GAMEPAD_NUMAXIS 8

NativeGLOBAL struct {
    int           State;
    int           DeviceId;
    char          DeviceName[DEVICE_NAME_LENGTH];
    char          DeviceGuid[DEVICE_GUID_LENGTH+1];
    int           NumAxes;
    int           NumButtons;
    float         Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS];
    unsigned char Button[NATIVE_ANDROID_GAMEPAD_NUMBUTTONS];
    unsigned char Hats;
} dmNativeJoy[ NATIVE_JOYSTICK_LAST + 1 ];


void dmNativeOSDiscoverJoysticks();

#endif // _platform_h_
