//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

#include <setjmp.h>

// This is the iOS version of Native
#define Native_IOS

#include <pthread.h>

#include "native.h"
#include "native_handles.h"
#if defined(__OBJC__)
#import <UIKit/UIKit.h>
#else
#include <objc/objc.h>
#endif


#ifndef GL_VERSION_3_0

typedef const GLubyte * (APIENTRY *PFNGLGETSTRINGIPROC) (GLenum, GLuint);

#endif /*GL_VERSION_3_0*/


//========================================================================
// Native platform specific types
//========================================================================

//------------------------------------------------------------------------
// Pointer length integer
//------------------------------------------------------------------------
typedef intptr_t Nativeintptr;

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
    int       portrait;        // GL_TRUE if window is in portrait mode

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

// ========= PLATFORM SPECIFIC PART ======================================

    id        window;
    id        pixelFormat;
    id        context;
    id        aux_context;
    id        delegate;
    id        view;
    id        viewController;
    unsigned int modifierFlags;
    int       frameBuffer;
};

NativeGLOBAL Nativewin dmNativeWin;


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------
NativeGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================


// ========= PLATFORM SPECIFIC PART ======================================


    // dlopen handle for dynamically-loading extension function pointers
    void *OpenGLFramework;

    int Unbundled;

    id DesktopMode;

    id AutoreleasePool;

} dmNativeLibrary;


//------------------------------------------------------------------------
// User input status (some of this should go in Nativewin)
//------------------------------------------------------------------------
NativeGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Mouse status
    int  MousePosX, MousePosY;
    int  MouseLeftButtonFromTouch, MousePositionFromTouch;
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

    float AccX, AccY, AccZ;

    // which touch is used for mouse emu.
    void *MouseEmulationTouch;

} dmNativeInput;

int dmNativeOSIsSceneActive(void);

#endif // _platform_h_
