//========================================================================
// GLFW - An OpenGL framework
// File:        internal.h
// Platform:    Any
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
#define GLFW_STICK              2


//========================================================================
// System independent include files
//========================================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


//------------------------------------------------------------------------
// Platform specific definitions goes in platform.h (which also includes
// glfw.h)
//------------------------------------------------------------------------

#include "platform.h"


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
// Window hints (set by glfwOpenWindowHint - will go into _GLFWthread)
//------------------------------------------------------------------------
typedef struct {
    int          RefreshRate;
    int          AccumRedBits;
    int          AccumGreenBits;
    int          AccumBlueBits;
    int          AccumAlphaBits;
    int          AuxBuffers;
    int          Stereo;
    int          WindowNoResize;
    int		 Samples;
} _GLFWhints;

GLFWGLOBAL _GLFWhints _glfwWinHints;


//------------------------------------------------------------------------
// Abstracted data stream (for image I/O)
//------------------------------------------------------------------------
typedef struct {
    FILE*     File;
    void*     Data;
    long      Position;
    long      Size;
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
int    _glfwPlatformExtensionSupported( const char *extension );
void * _glfwPlatformGetProcAddress( const char *procname );

// Joystick
int _glfwPlatformGetJoystickParam( int joy, int param );
int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes );
int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons );

// Threads
GLFWthread _glfwPlatformCreateThread( GLFWthreadfun fun, void *arg );
void       _glfwPlatformDestroyThread( GLFWthread ID );
int        _glfwPlatformWaitThread( GLFWthread ID, int waitmode );
GLFWthread _glfwPlatformGetThreadID( void );
GLFWmutex  _glfwPlatformCreateMutex( void );
void       _glfwPlatformDestroyMutex( GLFWmutex mutex );
void       _glfwPlatformLockMutex( GLFWmutex mutex );
void       _glfwPlatformUnlockMutex( GLFWmutex mutex );
GLFWcond   _glfwPlatformCreateCond( void );
void       _glfwPlatformDestroyCond( GLFWcond cond );
void       _glfwPlatformWaitCond( GLFWcond cond, GLFWmutex mutex, double timeout );
void       _glfwPlatformSignalCond( GLFWcond cond );
void       _glfwPlatformBroadcastCond( GLFWcond cond );
int        _glfwPlatformGetNumberOfProcessors( void );

// Time
double _glfwPlatformGetTime( void );
void   _glfwPlatformSetTime( double time );
void   _glfwPlatformSleep( double time );

// Window management
int  _glfwPlatformOpenWindow( int width, int height, int redbits, int greenbits, int bluebits, int alphabits, int depthbits, int stencilbits, int mode, _GLFWhints* hints );
void _glfwPlatformCloseWindow( void );
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

// Threads (thread.c)
_GLFWthread * _glfwGetThreadPointer( int ID );
void _glfwAppendThread( _GLFWthread * t );
void _glfwRemoveThread( _GLFWthread * t );

// OpenGL extensions (glext.c)
int _glfwStringInExtensionString( const char *string, const GLubyte *extensions );

// Abstracted data streams (stream.c)
int  _glfwOpenFileStream( _GLFWstream *stream, const char *name, const char *mode );
int  _glfwOpenBufferStream( _GLFWstream *stream, void *data, long size );
long _glfwReadStream( _GLFWstream *stream, void *data, long size );
long _glfwTellStream( _GLFWstream *stream );
int  _glfwSeekStream( _GLFWstream *stream, long offset, int whence );
void _glfwCloseStream( _GLFWstream *stream );

// Targa image I/O (tga.c)
int  _glfwReadTGA( _GLFWstream *s, GLFWimage *img, int flags );


#endif // _internal_h_
