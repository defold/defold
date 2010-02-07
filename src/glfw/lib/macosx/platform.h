//========================================================================
// GLFW - An OpenGL framework
// File:        platform.h
// Platform:    Mac OS X
// API Version: 2.6
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


// This is the Mac OS X version of GLFW
#define _GLFW_MAC_OS_X


// Include files
#include <Carbon/Carbon.h>
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#include <sched.h>
#include <pthread.h>
#include <sys/sysctl.h>
#include "../../include/GL/glfw.h"


//========================================================================
// Defines
//========================================================================

#define _GLFW_MAX_PATH_LENGTH (8192)

#define MAC_KEY_ENTER       0x24
#define MAC_KEY_RETURN      0x34
#define MAC_KEY_ESC         0x35
#define MAC_KEY_F1          0x7A
#define MAC_KEY_F2          0x78
#define MAC_KEY_F3          0x63
#define MAC_KEY_F4          0x76
#define MAC_KEY_F5          0x60
#define MAC_KEY_F6          0x61
#define MAC_KEY_F7          0x62
#define MAC_KEY_F8          0x64
#define MAC_KEY_F9          0x65
#define MAC_KEY_F10         0x6D
#define MAC_KEY_F11         0x67
#define MAC_KEY_F12         0x6F
#define MAC_KEY_F13         0x69
#define MAC_KEY_F14         0x6B
#define MAC_KEY_F15         0x71
#define MAC_KEY_UP          0x7E
#define MAC_KEY_DOWN        0x7D
#define MAC_KEY_LEFT        0x7B
#define MAC_KEY_RIGHT       0x7C
#define MAC_KEY_TAB         0x30
#define MAC_KEY_BACKSPACE   0x33
#define MAC_KEY_HELP        0x72
#define MAC_KEY_DEL         0x75
#define MAC_KEY_PAGEUP      0x74
#define MAC_KEY_PAGEDOWN    0x79
#define MAC_KEY_HOME        0x73
#define MAC_KEY_END         0x77
#define MAC_KEY_KP_0        0x52
#define MAC_KEY_KP_1        0x53
#define MAC_KEY_KP_2        0x54
#define MAC_KEY_KP_3        0x55
#define MAC_KEY_KP_4        0x56
#define MAC_KEY_KP_5        0x57
#define MAC_KEY_KP_6        0x58
#define MAC_KEY_KP_7        0x59
#define MAC_KEY_KP_8        0x5B
#define MAC_KEY_KP_9        0x5C
#define MAC_KEY_KP_DIVIDE   0x4B
#define MAC_KEY_KP_MULTIPLY 0x43
#define MAC_KEY_KP_SUBTRACT 0x4E
#define MAC_KEY_KP_ADD      0x45
#define MAC_KEY_KP_DECIMAL  0x41
#define MAC_KEY_KP_EQUAL    0x51
#define MAC_KEY_KP_ENTER    0x4C

//========================================================================
// full-screen/desktop-window "virtual" function table
//========================================================================

typedef int  ( * GLFWmacopenwindowfun )( int, int, int, int, int, int, int, int, int, int, int, int, int, int, int );
typedef void ( * GLFWmacclosewindowfun )( void );
typedef void ( * GLFWmacsetwindowtitlefun )( const char * );
typedef void ( * GLFWmacsetwindowsizefun )( int, int );
typedef void ( * GLFWmacsetwindowposfun )( int, int );
typedef void ( * GLFWmaciconifywindowfun )( void );
typedef void ( * GLFWmacrestorewindowfun )( void );
typedef void ( * GLFWmacrefreshwindowparamsfun )( void );
typedef void ( * GLFWmacsetmousecursorposfun )( int, int );

typedef struct
{
    GLFWmacopenwindowfun          OpenWindow;
    GLFWmacclosewindowfun         CloseWindow;
    GLFWmacsetwindowtitlefun      SetWindowTitle;
    GLFWmacsetwindowsizefun       SetWindowSize;
    GLFWmacsetwindowposfun        SetWindowPos;
    GLFWmaciconifywindowfun       IconifyWindow;
    GLFWmacrestorewindowfun       RestoreWindow;
    GLFWmacrefreshwindowparamsfun RefreshWindowParams;
    GLFWmacsetmousecursorposfun   SetMouseCursorPos;
}
_GLFWmacwindowfunctions;


//========================================================================
// Global variables (GLFW internals)
//========================================================================

GLFWGLOBAL CFDictionaryRef _glfwDesktopVideoMode;

//------------------------------------------------------------------------
// Window structure
//------------------------------------------------------------------------
typedef struct _GLFWwin_struct _GLFWwin;

struct _GLFWwin_struct {

    // ========= PLATFORM INDEPENDENT MANDATORY PART =========================

    // Window states
    int       Opened;          // Flag telling if window is opened or not
    int       Active;          // Application active flag
    int       Iconified;       // Window iconified flag

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
    int       RefreshRate;     // Refresh rate (for fullscreen mode)
    int       WindowNoResize;  // Resize- and maximize gadgets disabled flag
    int	      Samples;

    // Window status
    int       Width, Height;   // Window width and heigth

    // Extensions & OpenGL version
    int       Has_GL_SGIS_generate_mipmap;
    int       Has_GL_ARB_texture_non_power_of_two;
    int       GLVerMajor,GLVerMinor;


    // ========= PLATFORM SPECIFIC PART ======================================

    WindowRef               MacWindow;
    AGLContext              AGLContext;
    CGLContextObj           CGLContext;

    EventHandlerUPP         MouseUPP;
    EventHandlerUPP         CommandUPP;
    EventHandlerUPP         KeyboardUPP;
    EventHandlerUPP         WindowUPP;

    _GLFWmacwindowfunctions* WindowFunctions;

    // for easy access by _glfwPlatformGetWindowParam
    int Accelerated;
    int RedBits, GreenBits, BlueBits, AlphaBits;
    int DepthBits;
    int StencilBits;
    int AccumRedBits, AccumGreenBits, AccumBlueBits, AccumAlphaBits;
    int AuxBuffers;
    int Stereo;
};

GLFWGLOBAL _GLFWwin _glfwWin;


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


    // ========= PLATFORM SPECIFIC PART ======================================

    UInt32 Modifiers;

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


//------------------------------------------------------------------------
// Library global data
//------------------------------------------------------------------------
GLFWGLOBAL struct {

    // Timer data
    struct {
	double       t0;
    } Timer;

    struct {
	    // Bundle for dynamically-loading extension function pointers
    	CFBundleRef OpenGLFramework;
    } Libs;
    
    int Unbundled;
    
} _glfwLibrary;



//========================================================================
// Macros for encapsulating critical code sections (i.e. making parts
// of GLFW thread safe)
//========================================================================

// Define so we can use the same thread code as X11
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

// Thread list management
#define ENTER_THREAD_CRITICAL_SECTION \
pthread_mutex_lock( &_glfwThrd.CriticalSection );
#define LEAVE_THREAD_CRITICAL_SECTION \
pthread_mutex_unlock( &_glfwThrd.CriticalSection );


//========================================================================
// Prototypes for platform specific internal functions
//========================================================================

void  _glfwChangeToResourcesDirectory( void );

int  _glfwInstallEventHandlers( void );

//========================================================================
// Prototypes for full-screen/desktop-window "virtual" functions
//========================================================================

int  _glfwMacFSOpenWindow( int width, int height, int redbits, int greenbits, int bluebits, int alphabits, int depthbits, int stencilbits, int accumredbits, int accumgreenbits, int accumbluebits, int accumalphabits, int auxbuffers, int stereo, int refreshrate );
void _glfwMacFSCloseWindow( void );
void _glfwMacFSSetWindowTitle( const char *title );
void _glfwMacFSSetWindowSize( int width, int height );
void _glfwMacFSSetWindowPos( int x, int y );
void _glfwMacFSIconifyWindow( void );
void _glfwMacFSRestoreWindow( void );
void _glfwMacFSRefreshWindowParams( void );
void _glfwMacFSSetMouseCursorPos( int x, int y );

int  _glfwMacDWOpenWindow( int width, int height, int redbits, int greenbits, int bluebits, int alphabits, int depthbits, int stencilbits, int accumredbits, int accumgreenbits, int accumbluebits, int accumalphabits, int auxbuffers, int stereo, int refreshrate );
void _glfwMacDWCloseWindow( void );
void _glfwMacDWSetWindowTitle( const char *title );
void _glfwMacDWSetWindowSize( int width, int height );
void _glfwMacDWSetWindowPos( int x, int y );
void _glfwMacDWIconifyWindow( void );
void _glfwMacDWRestoreWindow( void );
void _glfwMacDWRefreshWindowParams( void );
void _glfwMacDWSetMouseCursorPos( int x, int y );

#endif // _platform_h_
