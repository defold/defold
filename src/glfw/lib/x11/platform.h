//========================================================================
// GLFW - An OpenGL framework
// File:        platform.h
// Platform:    X11 (Unix)
// API version: 2.6
// WWW:         http://glfw.sourceforge.net
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Camilla Berglund
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
#define _GLFW_X11


// Include files
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include "../../include/GL/glfw.h"

// Do we have pthread support?
#ifdef _GLFW_HAS_PTHREAD
 #include <pthread.h>
 #include <sched.h>
#endif

// With XFree86, we can use the XF86VidMode extension
#if defined( _GLFW_HAS_XF86VIDMODE )
 #include <X11/extensions/xf86vmode.h>
#endif

#if defined( _GLFW_HAS_XRANDR )
 #include <X11/extensions/Xrandr.h>
#endif

// Do we have support for dlopen/dlsym?
#if defined( _GLFW_HAS_DLOPEN )
 #include <dlfcn.h>
#endif

// We support two different ways for getting the number of processors in
// the system: sysconf (POSIX) and sysctl (BSD?)
#if defined( _GLFW_HAS_SYSCONF )

 // Use a single constant for querying number of online processors using
 // the sysconf function (e.g. SGI defines _SC_NPROC_ONLN instead of
 // _SC_NPROCESSORS_ONLN)
 #ifndef _SC_NPROCESSORS_ONLN
  #ifdef  _SC_NPROC_ONLN
   #define _SC_NPROCESSORS_ONLN _SC_NPROC_ONLN
  #else
   #error POSIX constant _SC_NPROCESSORS_ONLN not defined!
  #endif
 #endif

 // Macro for querying the number of processors
 #define _glfw_numprocessors(n) n=(int)sysconf(_SC_NPROCESSORS_ONLN)

#elif defined( _GLFW_HAS_SYSCTL )

 #include <sys/types.h>
 #include <sys/sysctl.h>

 // Macro for querying the number of processors
 #define _glfw_numprocessors(n) { \
    int mib[2], ncpu; \
    size_t len = 1; \
    mib[0] = CTL_HW; \
    mib[1] = HW_NCPU; \
    n      = 1; \
    if( sysctl( mib, 2, &ncpu, &len, NULL, 0 ) != -1 ) \
    { \
        if( len > 0 ) \
        { \
            n = ncpu; \
        } \
    } \
 }

#else

 // If neither sysconf nor sysctl is supported, assume single processor
 // system
 #define _glfw_numprocessors(n) n=1

#endif

void (*glXGetProcAddress(const GLubyte *procName))();
void (*glXGetProcAddressARB(const GLubyte *procName))();
void (*glXGetProcAddressEXT(const GLubyte *procName))();

// We support four different ways for getting addresses for GL/GLX
// extension functions: glXGetProcAddress, glXGetProcAddressARB,
// glXGetProcAddressEXT, and dlsym
#if   defined( _GLFW_HAS_GLXGETPROCADDRESSARB )
 #define _glfw_glXGetProcAddress(x) glXGetProcAddressARB(x)
#elif defined( _GLFW_HAS_GLXGETPROCADDRESS )
 #define _glfw_glXGetProcAddress(x) glXGetProcAddress(x)
#elif defined( _GLFW_HAS_GLXGETPROCADDRESSEXT )
 #define _glfw_glXGetProcAddress(x) glXGetProcAddressEXT(x)
#elif defined( _GLFW_HAS_DLOPEN )
 #define _glfw_glXGetProcAddress(x) dlsym(_glfwLibs.libGL,x)
 #define _GLFW_DLOPEN_LIBGL
#else
#define _glfw_glXGetProcAddress(x) NULL
#endif

// glXSwapIntervalSGI typedef (X11 buffer-swap interval control)
typedef int ( * GLXSWAPINTERVALSGI_T) (int interval);


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
    GLFWwindowsizefun    WindowSizeCallback;
    GLFWwindowclosefun   WindowCloseCallback;
    GLFWwindowrefreshfun WindowRefreshCallback;
    GLFWmousebuttonfun   MouseButtonCallback;
    GLFWmouseposfun      MousePosCallback;
    GLFWmousewheelfun    MouseWheelCallback;
    GLFWkeyfun           KeyCallback;
    GLFWcharfun          CharCallback;

    // User selected window settings
    int       Fullscreen;      // Fullscreen flag
    int       MouseLock;       // Mouse-lock flag
    int       AutoPollEvents;  // Auto polling flag
    int       SysKeysDisabled; // System keys disabled flag
    int       WindowNoResize;  // Resize- and maximize gadgets disabled flag

    // Window status & parameters
    int       Opened;          // Flag telling if window is opened or not
    int       Active;          // Application active flag
    int       Iconified;       // Window iconified flag
    int       Width, Height;   // Window width and heigth
    int       Accelerated;     // GL_TRUE if window is HW accelerated
    int       RedBits;
    int       GreenBits;
    int       BlueBits;
    int       AlphaBits;
    int       DepthBits;
    int       StencilBits;
    int       AccumRedBits;
    int       AccumGreenBits;
    int       AccumBlueBits;
    int       AccumAlphaBits;
    int       AuxBuffers;
    int       Stereo;
    int       RefreshRate;     // Vertical monitor refresh rate
    int       Samples;

    // Extensions & OpenGL version
    int       Has_GL_SGIS_generate_mipmap;
    int       Has_GL_ARB_texture_non_power_of_two;
    int       GLVerMajor,GLVerMinor;


// ========= PLATFORM SPECIFIC PART ======================================

    // Platform specific window resources
    Window      Win;             // Window
    int         Scrn;            // Screen ID
    XVisualInfo *VI;             // Visual
    GLXContext  CX;              // OpenGL rendering context
    Atom        WMDeleteWindow;  // For WM close detection
    Atom        WMPing;          // For WM ping response
    XSizeHints  *Hints;          // WM size hints

    // Platform specific extensions
    GLXSWAPINTERVALSGI_T SwapInterval;

    // Various platform specific internal variables
    int         OverrideRedirect; // True if window is OverrideRedirect
    int         KeyboardGrabbed; // True if keyboard is currently grabbed
    int         PointerGrabbed;  // True if pointer is currently grabbed
    int         PointerHidden;   // True if pointer is currently hidden
    int         MapNotifyCount;  // Used for during processing
    int         FocusInCount;    // Used for during processing

    // Screensaver data
    struct {
	int     Changed;
	int     Timeout;
	int     Interval;
	int     Blanking;
	int     Exposure;
    } Saver;

    // Fullscreen data
    struct {
	int     ModeChanged;
#if defined( _GLFW_HAS_XF86VIDMODE )
	XF86VidModeModeInfo OldMode;
#endif
#if defined( _GLFW_HAS_XRANDR )
        SizeID   OldSizeID;
	int      OldWidth;
	int      OldHeight;
	Rotation OldRotation;
#endif
    } FS;
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


// ========= PLATFORM SPECIFIC PART ======================================

    // Platform specific internal variables
    int  MouseMoved, CursorPosX, CursorPosY;

} _glfwInput;


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------
GLFWGLOBAL struct {

// ========= PLATFORM SPECIFIC PART ======================================

    Display     *Dpy;
    int         NumScreens;
    int         DefaultScreen;

    struct {
	int	Available;
	int     EventBase;
	int     ErrorBase;
    } XF86VidMode;

    struct {
	int	Available;
	int     EventBase;
	int     ErrorBase;
    } XRandR;

    // Timer data
    struct {
	double       Resolution;
	long long    t0;
    } Timer;

#if defined(_GLFW_DLOPEN_LIBGL)
    struct {
	void        *libGL;          // dlopen handle for libGL.so
    } Libs;
#endif
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
GLFWGLOBAL struct {
    int           Present;
    int           fd;
    int           NumAxes;
    int           NumButtons;
    float         *Axis;
    unsigned char *Button;
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

// Cursor handling
Cursor _glfwCreateNULLCursor( Display *display, Window root );

// Joystick input
void _glfwInitJoysticks( void );
void _glfwTerminateJoysticks( void );

// Unicode support
long _glfwKeySym2Unicode( KeySym keysym );


#endif // _platform_h_
