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

#ifndef _platform_h_
#define _platform_h_


// This is the X11 version of GLFW
#define _GLFW_ANDROID

// Include files
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <android_native_app_glue.h>
#include "../../include/GL/glfw.h"
#include "../../include/GL/glfw_native.h"

// Do we have pthread support?
#ifdef _GLFW_HAS_PTHREAD
 #include <pthread.h>
 #include <sched.h>
#endif

#define _glfw_numprocessors(n) n=1

// Pointer length integer
// One day, this will most likely move into glfw.h
typedef intptr_t GLFWintptr;

#ifndef GL_VERSION_3_0
typedef const GLubyte * (APIENTRY *PFNGLGETSTRINGIPROC) (GLenum, GLuint);
#endif /*GL_VERSION_3_0*/

//========================================================================
// Global variables (GLFW internals)
//========================================================================

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

    PFNGLGETSTRINGIPROC GetStringi;


// ========= PLATFORM SPECIFIC PART ======================================

    EGLDisplay display;
    EGLContext context;
    EGLContext aux_context;
    EGLConfig config;
    EGLSurface surface;
    EGLSurface aux_surface;
    struct android_app* app;
    // pipe used to go from java thread to native (JNI)
    int m_Pipefd[2];
    int paused;
    int hasSurface;
};

GLFWGLOBAL _GLFWwin _glfwWin;


//------------------------------------------------------------------------
// User input status (most of this should go in _GLFWwin)
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

    // Platform specific internal variables
    int  MouseMoved, CursorPosX, CursorPosY;
    float AccX, AccY, AccZ;

} _glfwInput;


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
        double      resolution;
        long long   t0;
    } Timer;

} _glfwLibrary;

//------------------------------------------------------------------------
// Thread record (one for each thread)
//------------------------------------------------------------------------
typedef struct _GLFWthread_struct _GLFWthread;

struct _GLFWthread_struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Pointer to previous and next threads in linked list
    _GLFWthread   *Previous, *Next;

    // GLFW user side thread information
    GLFWthread    ID;
    GLFWthreadfun Function;

// ========= PLATFORM SPECIFIC PART ======================================

    // System side thread information
#ifdef _GLFW_HAS_PTHREAD
    pthread_t     PosixID;
#endif

};


//------------------------------------------------------------------------
// General thread information
//------------------------------------------------------------------------
GLFWGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Next thread ID to use (increments for every created thread)
    GLFWthread       NextID;

    // First thread in linked list (always the main thread)
    _GLFWthread      First;

// ========= PLATFORM SPECIFIC PART ======================================

    // Critical section lock
#ifdef _GLFW_HAS_PTHREAD
    pthread_mutex_t  CriticalSection;
#endif

} _glfwThrd;


//------------------------------------------------------------------------
// Joystick information & state
//------------------------------------------------------------------------

#define DEVICE_ID_LENGTH 64

GLFWGLOBAL struct {
    int           Present;
    int           fd;
    int           NumAxes;
    int           NumButtons;
    float         *Axis;
    unsigned char *Button;
    char DeviceId[DEVICE_ID_LENGTH];
} _glfwJoy[ GLFW_JOYSTICK_LAST + 1 ];


//========================================================================
// Macros for encapsulating critical code sections (i.e. making parts
// of GLFW thread safe)
//========================================================================

// Thread list management
#ifdef _GLFW_HAS_PTHREAD
 #define ENTER_THREAD_CRITICAL_SECTION \
         pthread_mutex_lock( &_glfwThrd.CriticalSection );
 #define LEAVE_THREAD_CRITICAL_SECTION \
         pthread_mutex_unlock( &_glfwThrd.CriticalSection );
#else
 #define ENTER_THREAD_CRITICAL_SECTION
 #define LEAVE_THREAD_CRITICAL_SECTION
#endif


//========================================================================
// Prototypes for platform specific internal functions
//========================================================================

// Time
void _glfwInitTimer( void );

// Fullscreen support
int  _glfwGetClosestVideoMode( int screen, int *width, int *height, int *rate );
void _glfwSetVideoModeMODE( int screen, int mode, int rate );
void _glfwSetVideoMode( int screen, int *width, int *height, int *rate );
void _glfwRestoreVideoMode( void );

// Joystick input
void _glfwInitJoysticks( void );
void _glfwTerminateJoysticks( void );

#endif // _platform_h_
