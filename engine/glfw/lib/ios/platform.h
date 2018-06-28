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

#ifndef _platform_h_
#define _platform_h_

#include <setjmp.h>

// This is the iOS version of GLFW
#define _GLFW_IOS

#include <pthread.h>

#include "../../include/GL/glfw.h"
#include "../../include/GL/glfw_native.h"
#if defined(__OBJC__)
#import <UIKit/UIKit.h>
#else
#include <objc/objc.h>
#endif


#ifndef GL_VERSION_3_0

typedef const GLubyte * (APIENTRY *PFNGLGETSTRINGIPROC) (GLenum, GLuint);

#endif /*GL_VERSION_3_0*/


//========================================================================
// GLFW platform specific types
//========================================================================

//------------------------------------------------------------------------
// Pointer length integer
//------------------------------------------------------------------------
typedef intptr_t GLFWintptr;

//------------------------------------------------------------------------
// Window structure
//------------------------------------------------------------------------
typedef struct _GLFWwin_struct _GLFWwin;

struct _GLFWwin_struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // User callback functions
    GLFWwindowsizefun    windowSizeCallback;
    GLFWwindowclosefun   windowCloseCallback;
    GLFWwindowrefreshfun windowRefreshCallback;
    GLFWwindowfocusfun   windowFocusCallback;
    GLFWmousebuttonfun   mouseButtonCallback;
    GLFWmouseposfun      mousePosCallback;
    GLFWmousewheelfun    mouseWheelCallback;
    GLFWkeyfun           keyCallback;
    GLFWcharfun          charCallback;
    GLFWmarkedtextfun    markedTextCallback;
    GLFWgamepadfun       gamepadCallback;

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

    // Misc
    jmp_buf   bailEventLoopBuf;
    jmp_buf   finishInitBuf;
};

GLFWGLOBAL _GLFWwin _glfwWin;


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------
GLFWGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Window opening hints
    _GLFWhints      hints;

// ========= PLATFORM SPECIFIC PART ======================================

    // Timer data
    struct {
        double t0;
    } Timer;

    // dlopen handle for dynamically-loading extension function pointers
    void *OpenGLFramework;

    int Unbundled;

    id DesktopMode;

    id AutoreleasePool;

} _glfwLibrary;


//------------------------------------------------------------------------
// User input status (some of this should go in _GLFWwin)
//------------------------------------------------------------------------
GLFWGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Mouse status
    int  MousePosX, MousePosY;
    int  WheelPos;
    char MouseButton[ GLFW_MOUSE_BUTTON_LAST+1 ];

    // Keyboard status
    char Key[ GLFW_KEY_LAST+1 ];
    int  LastChar;

    // User selected settings
    int  StickyKeys;
    int  StickyMouseButtons;
    int  KeyRepeat;

    GLFWTouch Touch[GLFW_MAX_TOUCH];

// ========= PLATFORM SPECIFIC PART ======================================

    float AccX, AccY, AccZ;

    // which touch is used for mouse emu.
    void *MouseEmulationTouch;

} _glfwInput;

//------------------------------------------------------------------------
// Thread information
//------------------------------------------------------------------------
typedef struct _GLFWthread_struct _GLFWthread;

// Thread record (one for each thread)
struct _GLFWthread_struct {

    // Pointer to previous and next threads in linked list
    _GLFWthread   *Previous, *Next;

    // GLFW user side thread information
    GLFWthread    ID;
    GLFWthreadfun Function;

    // System side thread information
    pthread_t     PosixID;
};

// General thread information
GLFWGLOBAL struct {

    // Critical section lock
    pthread_mutex_t  CriticalSection;

    // Next thread ID to use (increments for every created thread)
    GLFWthread       NextID;

    // First thread in linked list (always the main thread)
    _GLFWthread      First;

} _glfwThrd;


//========================================================================
// Macros for encapsulating critical code sections (i.e. making parts
// of GLFW thread safe)
//========================================================================

// Define so we can use the same thread code as X11
#define _glfw_numprocessors(n) n = 1

// Thread list management
#define ENTER_THREAD_CRITICAL_SECTION \
pthread_mutex_lock( &_glfwThrd.CriticalSection );
#define LEAVE_THREAD_CRITICAL_SECTION \
pthread_mutex_unlock( &_glfwThrd.CriticalSection );


#endif // _platform_h_
