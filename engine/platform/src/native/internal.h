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


// Modified for Defold: private mobile/web backend, without the GLFW API.
#ifndef _internal_h_
#define _internal_h_

//========================================================================
// NativeGLOBAL is a macro that places all global variables in the init.c
// module (all other modules reference global variables as 'extern')
//========================================================================

#if defined( _init_c_ )
#define NativeGLOBAL
#else
#define NativeGLOBAL extern
#endif


// Internal key and button state/action definitions
#define NATIVE_STICK 2
#define NATIVE_CLICKED 3


//========================================================================
// System independent include files
//========================================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


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
} Nativewndconfig;


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
    Nativeintptr  platformID;
} Nativefbconfig;


// Flag indicating if Native has been initialized
#if defined( _init_c_ )
int dmNativeInitialized = 0;
#else
NativeGLOBAL int dmNativeInitialized;
#endif


// Init/terminate
int dmNativeOSInit( void );
int dmNativeOSTerminate( void );

// OpenGL extensions
int dmNativeOSExtensionSupported( const char *extension );
void * dmNativeOSGetProcAddress( const char *procname );

// Joystick
int dmNativeOSGetJoystickParam( int joy, int param );
int dmNativeOSGetJoystickPos( int joy, float *pos, int numaxes );
int dmNativeOSGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons );
int dmNativeOSGetJoystickHats( int joy, unsigned char *hats, int numhats );
int dmNativeOSGetJoystickDeviceId( int joy, char** device_id );

// Window management
int  dmNativeOSOpenWindow( int width, int height, const Nativewndconfig *wndconfig, const Nativefbconfig *fbconfig );
int  dmNativeOSOpenWindowVulkan( int width, int height, const Nativewndconfig *wndconfig, const Nativefbconfig *fbconfig );
int  dmNativeOSOpenWindowOpenGL( int width, int height, const Nativewndconfig *wndconfig, const Nativefbconfig *fbconfig );
void dmNativeOSCloseWindow( void );
int  dmNativeOSGetWindowRefreshRate( void );
int  dmNativeOSGetDefaultFramebuffer( void );
void dmNativeOSSetWindowTitle( const char *title );
void dmNativeOSSetWindowSize( int width, int height );
void dmNativeOSSetWindowPos( int x, int y );
void dmNativeOSIconifyWindow( void );
void dmNativeOSRestoreWindow( void );
void dmNativeOSSwapBuffers( void );
void dmNativeOSSwapInterval( int interval );
void dmNativeOSRefreshWindowParams( void );
void dmNativeOSPollEvents( void );
void dmNativeOSWaitEvents( void );
void dmNativeOSHideMouseCursor( void );
void dmNativeOSShowMouseCursor( void );
void dmNativeOSSetMouseCursorPos( int x, int y );

// Defold extensions
int dmNativeOSGetAcceleration(float* x, float* y, float* z);
int dmNativeOSQueryAuxContext();
int dmNativeOSQueryAuxContextVulkan();
int dmNativeOSQueryAuxContextOpenGL();
void* dmNativeOSAcquireAuxContext();
void* dmNativeOSAcquireAuxContextVulkan();
void* dmNativeOSAcquireAuxContextOpenGL();
void dmNativeOSUnacquireAuxContext(void* context);
void dmNativeOSUnacquireAuxContextVulkan(void* context);
void dmNativeOSUnacquireAuxContextOpenGL(void* context);
void dmNativeOSSetViewType(int view_type);
void dmNativeOSSetWindowBackgroundColor(unsigned int color);
float dmNativeOSGetDisplayScaleFactor();

// Input handling (window.c)
void dmNativeClearInput( void );
void dmNativeInputDeactivation( void );
void dmNativeInputKey( int key, int action );
void dmNativeInputChar( int character, int action );
void dmNativeInputMouseClick( int button, int action );
void dmNativeSetMarkedText( char* str );
void dmNativeShowKeyboard(int show, int type, int auto_close);
void dmNativeResetKeyboard( void );

// OpenGL extensions (glext.c)
void dmNativeParseGLVersion( int *major, int *minor, int *rev );
void dmNativeRefreshContextParams( void );

// Joystick
void dmNativeTerminateJoysticks( void );

#endif // _internal_h_
