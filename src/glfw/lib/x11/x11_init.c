//========================================================================
// GLFW - An OpenGL framework
// File:        x11_init.c
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

#include "internal.h"



//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Initialize GLFW thread package
//========================================================================

static void _glfwInitThreads( void )
{
    // Initialize critical section handle
#ifdef _GLFW_HAS_PTHREAD
    (void) pthread_mutex_init( &_glfwThrd.CriticalSection, NULL );
#endif

    // The first thread (the main thread) has ID 0
    _glfwThrd.NextID = 0;

    // Fill out information about the main thread (this thread)
    _glfwThrd.First.ID       = _glfwThrd.NextID++;
    _glfwThrd.First.Function = NULL;
    _glfwThrd.First.Previous = NULL;
    _glfwThrd.First.Next     = NULL;
#ifdef _GLFW_HAS_PTHREAD
    _glfwThrd.First.PosixID  = pthread_self();
#endif
}


//========================================================================
// Terminate GLFW thread package
//========================================================================

static void _glfwTerminateThreads( void )
{
#ifdef _GLFW_HAS_PTHREAD

    _GLFWthread *t, *t_next;

    // Enter critical section
    ENTER_THREAD_CRITICAL_SECTION

    // Kill all threads (NOTE: THE USER SHOULD WAIT FOR ALL THREADS TO
    // DIE, _BEFORE_ CALLING glfwTerminate()!!!)
    t = _glfwThrd.First.Next;
    while( t != NULL )
    {
        // Get pointer to next thread
        t_next = t->Next;

        // Simply murder the process, no mercy!
        pthread_kill( t->PosixID, SIGKILL );

        // Free memory allocated for this thread
        free( (void *) t );

        // Select next thread in list
        t = t_next;
    }

    // Leave critical section
    LEAVE_THREAD_CRITICAL_SECTION

    // Delete critical section handle
    pthread_mutex_destroy( &_glfwThrd.CriticalSection );

#endif // _GLFW_HAS_PTHREAD
}


//========================================================================
// Dynamically load libraries
//========================================================================

#ifdef _GLFW_DLOPEN_LIBGL
static char * _glfw_libGL_name[ ] =
{
    "libGL.so",
    "libGL.so.1",
    "/usr/lib/libGL.so",
    "/usr/lib/libGL.so.1",
    NULL
};
#endif

static void _glfwInitLibraries( void )
{
#ifdef _GLFW_DLOPEN_LIBGL
    int i;

    _glfwLibrary.Libs.libGL = NULL;
    for( i = 0; !_glfw_libGL_name[ i ] != NULL; i ++ )
    {
        _glfwLibrary.Libs.libGL = dlopen( _glfw_libGL_name[ i ],
                                          RTLD_LAZY | RTLD_GLOBAL );
	if( _glfwLibrary.Libs.libGL )
	    break;
    }
#endif
}


//========================================================================
// Terminate GLFW when exiting application
//========================================================================

void _glfwTerminate_atexit( void )
{
    glfwTerminate();
}


//========================================================================
// Initialize X11 display
//========================================================================

static int _glfwInitDisplay( void )
{
    // Open display
    _glfwLibrary.Dpy = XOpenDisplay( 0 );
    if( !_glfwLibrary.Dpy )
    {
        return GL_FALSE;
    }

    // Check screens
    _glfwLibrary.NumScreens = ScreenCount( _glfwLibrary.Dpy );
    _glfwLibrary.DefaultScreen = DefaultScreen( _glfwLibrary.Dpy );

    // Check for XF86VidMode extension
#ifdef _GLFW_HAS_XF86VIDMODE
    _glfwLibrary.XF86VidMode.Available =
        XF86VidModeQueryExtension( _glfwLibrary.Dpy,
	                           &_glfwLibrary.XF86VidMode.EventBase,
	                           &_glfwLibrary.XF86VidMode.ErrorBase);
#else
    _glfwLibrary.XF86VidMode.Available = 0;
#endif

    // Check for XRandR extension
#ifdef _GLFW_HAS_XRANDR
    _glfwLibrary.XRandR.Available =
        XRRQueryExtension( _glfwLibrary.Dpy,
	                   &_glfwLibrary.XRandR.EventBase,
			   &_glfwLibrary.XRandR.ErrorBase );
#else
    _glfwLibrary.XRandR.Available = 0;
#endif

     return GL_TRUE;
}


//========================================================================
// Terminate X11 display
//========================================================================

static void _glfwTerminateDisplay( void )
{
    // Open display
    if( _glfwLibrary.Dpy )
    {
        XCloseDisplay( _glfwLibrary.Dpy );
        _glfwLibrary.Dpy = NULL;
    }
}


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Initialize various GLFW state
//========================================================================

int _glfwPlatformInit( void )
{
    // Initialize display
    if( !_glfwInitDisplay() )
    {
        return GL_FALSE;
    }

    // Initialize thread package
    _glfwInitThreads();

    // Try to load libGL.so if necessary
    _glfwInitLibraries();

    // Install atexit() routine
    atexit( _glfwTerminate_atexit );

    // Initialize joysticks
    _glfwInitJoysticks();

    // Start the timer
    _glfwInitTimer();

    return GL_TRUE;
}


//========================================================================
// Close window and kill all threads
//========================================================================

int _glfwPlatformTerminate( void )
{
#ifdef _GLFW_HAS_PTHREAD
    // Only the main thread is allowed to do this...
    if( pthread_self() != _glfwThrd.First.PosixID )
    {
        return GL_FALSE;
    }
#endif // _GLFW_HAS_PTHREAD

    // Close OpenGL window
    glfwCloseWindow();

    // Kill thread package
    _glfwTerminateThreads();

    // Terminate display
    _glfwTerminateDisplay();

    // Terminate joysticks
    _glfwTerminateJoysticks();

    // Unload libGL.so if necessary
#ifdef _GLFW_DLOPEN_LIBGL
    if( _glfwLibrary.Libs.libGL != NULL )
    {
        dlclose( _glfwLibrary.Libs.libGL );
        _glfwLibrary.Libs.libGL = NULL;
    }
#endif

    return GL_TRUE;
}

