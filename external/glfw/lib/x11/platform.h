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
#include "../../include/GL/glfw_native.h"

// Do we have pthread support?
#ifdef _GLFW_HAS_PTHREAD
 #include <pthread.h>
 #include <sched.h>
#endif

// We need declarations for GLX version 1.3 or above even if the server doesn't
// support version 1.3
#ifndef GLX_VERSION_1_3
 #error "GLX header version 1.3 or above is required"
#endif

#if defined( _GLFW_HAS_XF86VIDMODE ) && defined( _GLFW_HAS_XRANDR )
 #error "Xf86VidMode and RandR extensions cannot both be enabled"
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

// Pointer length integer
// One day, this will most likely move into glfw.h
typedef intptr_t GLFWintptr;


#ifndef GLX_SGI_swap_control

// Function signature for GLX_SGI_swap_control
typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);

#endif /*GLX_SGI_swap_control*/


#ifndef GLX_SGIX_fbconfig

/* Type definitions for GLX_SGIX_fbconfig */
typedef XID GLXFBConfigIDSGIX;
typedef struct __GLXFBConfigRec *GLXFBConfigSGIX;

/* Function signatures for GLX_SGIX_fbconfig */
typedef int ( * PFNGLXGETFBCONFIGATTRIBSGIXPROC) (Display *dpy, GLXFBConfigSGIX config, int attribute, int *value);
typedef GLXFBConfigSGIX * ( * PFNGLXCHOOSEFBCONFIGSGIXPROC) (Display *dpy, int screen, int *attrib_list, int *nelements);
typedef GLXContext ( * PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC) (Display *dpy, GLXFBConfigSGIX config, int render_type, GLXContext share_list, Bool direct);
typedef XVisualInfo * ( * PFNGLXGETVISUALFROMFBCONFIGSGIXPROC) (Display *dpy, GLXFBConfigSGIX config);

/* Tokens for GLX_SGIX_fbconfig */
#define GLX_WINDOW_BIT_SGIX                0x00000001
#define GLX_PIXMAP_BIT_SGIX                0x00000002
#define GLX_RGBA_BIT_SGIX                  0x00000001
#define GLX_COLOR_INDEX_BIT_SGIX           0x00000002
#define GLX_DRAWABLE_TYPE_SGIX             0x8010
#define GLX_RENDER_TYPE_SGIX               0x8011
#define GLX_X_RENDERABLE_SGIX              0x8012
#define GLX_FBCONFIG_ID_SGIX               0x8013
#define GLX_RGBA_TYPE_SGIX                 0x8014
#define GLX_COLOR_INDEX_TYPE_SGIX          0x8015
#define GLX_SCREEN_EXT                     0x800C

#endif /*GLX_SGIX_fbconfig*/


#ifndef GLX_ARB_create_context

/* Tokens for glXCreateContextAttribsARB attributes */
#define GLX_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB           0x2092
#define GLX_CONTEXT_FLAGS_ARB                   0x2094

/* Bits for WGL_CONTEXT_FLAGS_ARB */
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002

/* Prototype for glXCreateContextAttribs */
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)( Display *display, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);

#endif /*GLX_ARB_create_context*/


#ifndef GLX_ARB_create_context_profile

/* Tokens for glXCreateContextAttribsARB attributes */
#define GLX_CONTEXT_PROFILE_MASK_ARB            0x9126

/* BIts for GLX_CONTEXT_PROFILE_MASK_ARB */
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

#endif /*GLX_ARB_create_context_profile*/


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
    GLFWwindowiconifyfun windowIconifyCallback;
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

    // Platform specific window resources
    Colormap      colormap;          // Window colormap
    Window        window;            // Window
    Window        root;              // Root window for screen
    int           screen;            // Screen ID
    XVisualInfo  *visual;            // Visual for selected GLXFBConfig
    GLXFBConfigID fbconfigID;        // ID of selected GLXFBConfig
    GLXContext    context;           // OpenGL rendering context
    GLXContext    aux_context;       // Auxillary rendering context
    Atom          wmDeleteWindow;    // WM_DELETE_WINDOW atom
    Atom          wmPing;            // _NET_WM_PING atom
    Atom          wmState;           // _NET_WM_STATE atom
    Atom          wmStateFullscreen; // _NET_WM_STATE_FULLSCREEN atom
    Atom          wmActiveWindow;    // _NET_ACTIVE_WINDOW atom
    Cursor        cursor;            // Invisible cursor for hidden cursor

    // GLX extensions
    PFNGLXSWAPINTERVALSGIPROC             SwapIntervalSGI;
    PFNGLXGETFBCONFIGATTRIBSGIXPROC       GetFBConfigAttribSGIX;
    PFNGLXCHOOSEFBCONFIGSGIXPROC          ChooseFBConfigSGIX;
    PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC CreateContextWithConfigSGIX;
    PFNGLXGETVISUALFROMFBCONFIGSGIXPROC   GetVisualFromFBConfigSGIX;
    PFNGLXCREATECONTEXTATTRIBSARBPROC     CreateContextAttribsARB;
    GLboolean   has_GLX_SGIX_fbconfig;
    GLboolean   has_GLX_SGI_swap_control;
    GLboolean   has_GLX_ARB_multisample;
    GLboolean   has_GLX_ARB_create_context;
    GLboolean   has_GLX_ARB_create_context_profile;

    // Various platform specific internal variables
    GLboolean   hasEWMH;          // True if window manager supports EWMH
    GLboolean   overrideRedirect; // True if window is OverrideRedirect
    GLboolean   keyboardGrabbed;  // True if keyboard is currently grabbed
    GLboolean   pointerGrabbed;   // True if pointer is currently grabbed
    GLboolean   pointerHidden;    // True if pointer is currently hidden

    // Screensaver data
    struct {
        int     changed;
        int     timeout;
        int     interval;
        int     blanking;
        int     exposure;
    } Saver;

    // Fullscreen data
    struct {
        int     modeChanged;
#if defined( _GLFW_HAS_XF86VIDMODE )
        XF86VidModeModeInfo oldMode;
#endif
#if defined( _GLFW_HAS_XRANDR )
        SizeID   oldSizeID;
        int      oldWidth;
        int      oldHeight;
        Rotation oldRotation;
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

    GLFWTouch Touch[GLFW_MAX_TOUCH];

// ========= PLATFORM SPECIFIC PART ======================================

    // Platform specific internal variables
    int  MouseMoved, CursorPosX, CursorPosY;

} _glfwInput;


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------
GLFWGLOBAL struct {

// ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Window opening hints
    _GLFWhints      hints;

// ========= PLATFORM SPECIFIC PART ======================================

    Display        *display;

    // Server-side GLX version
    int             glxMajor, glxMinor;

    struct {
        int         available;
        int         eventBase;
        int         errorBase;
    } XF86VidMode;

    struct {
        int         available;
        int         eventBase;
        int         errorBase;
    } XRandR;

    // Timer data
    struct {
        double      resolution;
        long long   t0;
    } Timer;

#if defined(_GLFW_HAS_DLOPEN)
    struct {
        void       *libGL;  // dlopen handle for libGL.so
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

#define DEVICE_ID_LENGTH 64

GLFWGLOBAL struct {
    int           Present;
    int           fd;
    int           NumAxes;
    int           NumButtons;
    int           InputId;
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

// Unicode support
long _glfwKeySym2Unicode( KeySym keysym );


#endif // _platform_h_
