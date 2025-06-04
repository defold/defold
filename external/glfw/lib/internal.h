//========================================================================
// GLFW - An OpenGL framework
// Platform:    Any
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

#ifndef _internal_h_
#define _internal_h_

//========================================================================
// GLFWGLOBAL is a macro that places all global variables in the init.c
// module (all other modules reference global variables as 'extern')
//========================================================================

#if defined( _init_c_ )
#define GLFWGLOBAL
#else
#define GLFWGLOBAL extern
#endif


//========================================================================
// Input handling definitions
//========================================================================

// Internal key and button state/action definitions
#define GLFW_STICK 2
#define GLFW_CLICKED 3


//========================================================================
// System independent include files
//========================================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


//------------------------------------------------------------------------
// Window opening hints (set by glfwOpenWindowHint)
// A bucket of semi-random stuff bunched together for historical reasons
// This is used only by the platform independent code and only to store
// parameters passed to us by glfwOpenWindowHint
//------------------------------------------------------------------------
typedef struct {
    int         refreshRate;
    int         accumRedBits;
    int         accumGreenBits;
    int         accumBlueBits;
    int         accumAlphaBits;
    int         auxBuffers;
    int         stereo;
    int         windowNoResize;
    int         samples;
    int         glMajor;
    int         glMinor;
    int         glForward;
    int         glDebug;
    int         glProfile;
    int         highDPI;
    int         clientAPI;
} _GLFWhints;


//------------------------------------------------------------------------
// Platform specific definitions goes in platform.h (which also includes
// glfw.h)
//------------------------------------------------------------------------

#include "platform.h"


//------------------------------------------------------------------------
// Parameters relating to the creation of the context and window but not
// directly related to the properties of the framebuffer
// This is used to pass window and context creation parameters from the
// platform independent code to the platform specific code
//------------------------------------------------------------------------
typedef struct {
    int         mode;
    int         refreshRate;
    int         windowNoResize;
    int         glMajor;
    int         glMinor;
    int         glForward;
    int         glDebug;
    int         glProfile;
    int         highDPI;
    int         clientAPI;
} _GLFWwndconfig;


//------------------------------------------------------------------------
// Framebuffer configuration descriptor, i.e. buffers and their sizes
// Also a platform specific ID used to map back to the actual backend APIs
// This is used to pass framebuffer parameters from the platform independent
// code to the platform specific code, and also to enumerate and select
// available framebuffer configurations
//------------------------------------------------------------------------
typedef struct {
    int         redBits;
    int         greenBits;
    int         blueBits;
    int         alphaBits;
    int         depthBits;
    int         stencilBits;
    int         accumRedBits;
    int         accumGreenBits;
    int         accumBlueBits;
    int         accumAlphaBits;
    int         auxBuffers;
    int         stereo;
    int         samples;
    GLFWintptr  platformID;
} _GLFWfbconfig;


//========================================================================
// System independent global variables (GLFW internals)
//========================================================================

// Flag indicating if GLFW has been initialized
#if defined( _init_c_ )
int _glfwInitialized = 0;
#else
GLFWGLOBAL int _glfwInitialized;
#endif


//------------------------------------------------------------------------
// Abstract data stream (for image I/O)
//------------------------------------------------------------------------
typedef struct {
    FILE*   file;
    void*   data;
    long    position;
    long    size;
} _GLFWstream;


//========================================================================
// Prototypes for platform specific implementation functions
//========================================================================

// Init/terminate
int _glfwPlatformInit( void );
int _glfwPlatformTerminate( void );

// Enable/Disable
void _glfwPlatformEnableSystemKeys( void );
void _glfwPlatformDisableSystemKeys( void );

// Fullscreen
int  _glfwPlatformGetVideoModes( GLFWvidmode *list, int maxcount );
void _glfwPlatformGetDesktopMode( GLFWvidmode *mode );

// OpenGL extensions
int _glfwPlatformExtensionSupported( const char *extension );
void * _glfwPlatformGetProcAddress( const char *procname );

// Joystick
int _glfwPlatformGetJoystickParam( int joy, int param );
int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes );
int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons );
int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats );
int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id );

// Threads
GLFWthread _glfwPlatformCreateThread( GLFWthreadfun fun, void *arg );
void _glfwPlatformDestroyThread( GLFWthread ID );
int _glfwPlatformWaitThread( GLFWthread ID, int waitmode );
GLFWthread _glfwPlatformGetThreadID( void );
GLFWmutex _glfwPlatformCreateMutex( void );
void _glfwPlatformDestroyMutex( GLFWmutex mutex );
void _glfwPlatformLockMutex( GLFWmutex mutex );
void _glfwPlatformUnlockMutex( GLFWmutex mutex );
GLFWcond _glfwPlatformCreateCond( void );
void _glfwPlatformDestroyCond( GLFWcond cond );
void _glfwPlatformWaitCond( GLFWcond cond, GLFWmutex mutex, double timeout );
void _glfwPlatformSignalCond( GLFWcond cond );
void _glfwPlatformBroadcastCond( GLFWcond cond );
int _glfwPlatformGetNumberOfProcessors( void );

// Time
double _glfwPlatformGetTime( void );
void _glfwPlatformSetTime( double time );
void _glfwPlatformSleep( double time );

// Window management
int  _glfwPlatformOpenWindow( int width, int height, const _GLFWwndconfig *wndconfig, const _GLFWfbconfig *fbconfig );
int  _glfwPlatformOpenWindowVulkan( int width, int height, const _GLFWwndconfig *wndconfig, const _GLFWfbconfig *fbconfig );
int  _glfwPlatformOpenWindowOpenGL( int width, int height, const _GLFWwndconfig *wndconfig, const _GLFWfbconfig *fbconfig );
void _glfwPlatformCloseWindow( void );
int  _glfwPlatformGetWindowRefreshRate( void );
int  _glfwPlatformGetDefaultFramebuffer( void );
void _glfwPlatformSetWindowTitle( const char *title );
void _glfwPlatformSetWindowSize( int width, int height );
void _glfwPlatformSetWindowPos( int x, int y );
void _glfwPlatformIconifyWindow( void );
void _glfwPlatformRestoreWindow( void );
void _glfwPlatformSwapBuffers( void );
void _glfwPlatformSwapInterval( int interval );
void _glfwPlatformRefreshWindowParams( void );
void _glfwPlatformPollEvents( void );
void _glfwPlatformWaitEvents( void );
void _glfwPlatformHideMouseCursor( void );
void _glfwPlatformShowMouseCursor( void );
void _glfwPlatformSetMouseCursorPos( int x, int y );

// Defold extensions
int _glfwPlatformGetAcceleration(float* x, float* y, float* z);
int _glfwPlatformQueryAuxContext();
int _glfwPlatformQueryAuxContextVulkan();
int _glfwPlatformQueryAuxContextOpenGL();
void* _glfwPlatformAcquireAuxContext();
void* _glfwPlatformAcquireAuxContextVulkan();
void* _glfwPlatformAcquireAuxContextOpenGL();
void _glfwPlatformUnacquireAuxContext(void* context);
void _glfwPlatformUnacquireAuxContextVulkan(void* context);
void _glfwPlatformUnacquireAuxContextOpenGL(void* context);
void _glfwPlatformSetViewType(int view_type);
void _glfwPlatformSetWindowBackgroundColor(unsigned int color);
float _glfwPlatformGetDisplayScaleFactor();

//========================================================================
// Prototypes for platform independent internal functions
//========================================================================

// Window management (window.c)
void _glfwClearWindowHints( void );

// Input handling (window.c)
void _glfwClearInput( void );
void _glfwInputDeactivation( void );
void _glfwInputKey( int key, int action );
void _glfwInputChar( int character, int action );
void _glfwInputMouseClick( int button, int action );
void _glfwSetMarkedText( char* str );
void _glfwShowKeyboard(int show, int type, int auto_close);
void _glfwResetKeyboard( void );

// Threads (thread.c)
_GLFWthread * _glfwGetThreadPointer( int ID );
void _glfwAppendThread( _GLFWthread * t );
void _glfwRemoveThread( _GLFWthread * t );

// OpenGL extensions (glext.c)
void _glfwParseGLVersion( int *major, int *minor, int *rev );
int _glfwStringInExtensionString( const char *string, const GLubyte *extensions );
void _glfwRefreshContextParams( void );

// Abstracted data streams (stream.c)
int _glfwOpenFileStream( _GLFWstream *stream, const char *name, const char *mode );
int _glfwOpenBufferStream( _GLFWstream *stream, void *data, long size );
long _glfwReadStream( _GLFWstream *stream, void *data, long size );
long _glfwTellStream( _GLFWstream *stream );
int _glfwSeekStream( _GLFWstream *stream, long offset, int whence );
void _glfwCloseStream( _GLFWstream *stream );

// Targa image I/O (tga.c)
int _glfwReadTGA( _GLFWstream *s, GLFWimage *img, int flags );

// Framebuffer configs
const _GLFWfbconfig *_glfwChooseFBConfig( const _GLFWfbconfig *desired,
                                          const _GLFWfbconfig *alternatives,
                                          unsigned int count );

// Joystick
int _glfwInitJoysticks( void );
void _glfwTerminateJoysticks( void );

#endif // _internal_h_
