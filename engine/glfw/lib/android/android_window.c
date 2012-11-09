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

#include "internal.h"

#include <limits.h>

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and
// the OpenGL rendering context is created
//========================================================================

static int32_t handleInput(struct android_app* app, AInputEvent* event)
{
    struct engine* engine = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK ;
        int32_t x = AMotionEvent_getX(event, 0);
        int32_t y = AMotionEvent_getY(event, 0);
        _glfwInput.MousePosX = x;
        _glfwInput.MousePosY = y;

        switch (action)
        {
        case AMOTION_EVENT_ACTION_DOWN:
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
            break;
        case AMOTION_EVENT_ACTION_UP:
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            if( _glfwWin.mousePosCallback )
            {
                _glfwWin.mousePosCallback(x, y);
            }
            break;
        }
        return 1;
    }
    return 0;
}

int _glfwPlatformOpenWindow( int width__, int height__,
                             const _GLFWwndconfig* wndconfig__,
                             const _GLFWfbconfig* fbconfig__ )
{
    _glfwWin.app = _glfwAndrodApp;
    _glfwWin.app->onInputEvent = handleInput;

    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };

    /*
     * NOTE: The example simple_gles2 doesn't work with EGL_CONTEXT_CLIENT_VERSION
     * set to 2 in emulator. Might work on real device though
     */
    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE, EGL_NONE,
    };

    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(_glfwWin.app->window, 0, 0, format);
    surface = eglCreateWindowSurface(display, config, _glfwWin.app->window, NULL);
    context = eglCreateContext(display, config, NULL, contextAttribs);
    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        return GL_FALSE;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    _glfwWin.width = w;
    _glfwWin.height = h;
    if (_glfwWin.windowSizeCallback)
    {
        _glfwWin.windowSizeCallback(w, h);
    }

    _glfwWin.display = display;
    _glfwWin.context = context;
    _glfwWin.surface = surface;

    _glfwWin.width = w;
    _glfwWin.height = h;

    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    if (_glfwWin.display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(_glfwWin.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (_glfwWin.context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(_glfwWin.display, _glfwWin.context);
        }
        if (_glfwWin.surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(_glfwWin.display, _glfwWin.surface);
        }
        eglTerminate(_glfwWin.display);
    }

    _glfwWin.display = EGL_NO_DISPLAY;
    _glfwWin.context = EGL_NO_CONTEXT;
    _glfwWin.surface = EGL_NO_SURFACE;
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position.
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
}

//========================================================================
// Window iconification
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
}

//========================================================================
// Window un-iconification
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}

//========================================================================
// Swap OpenGL buffers and poll any new events
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    eglSwapBuffers(_glfwWin.display, _glfwWin.surface);
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    eglSwapInterval(_glfwWin.display, interval);
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}

//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
   int ident;
   int events;
   struct android_poll_source* source;

   while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0)
   {
       // Process this event.
       if (source != NULL) {
           source->process(_glfwWin.app, source);
       }
   }
}

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}
