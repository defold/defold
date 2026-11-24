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
#include "internal.h"


//************************************************************************
//****                  Native internal functions                       ****
//************************************************************************

#ifndef GL_VERSION_3_0
#define GL_NUM_EXTENSIONS                 0x821D
#define GL_CONTEXT_FLAGS                  0x821E
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x0001
#endif

#if !defined(GL_MAJOR_VERSION)
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#endif

#ifndef GL_VERSION_3_2
#define GL_CONTEXT_CORE_PROFILE_BIT       0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_CONTEXT_PROFILE_MASK           0x9126
#endif

#if defined(DM_PLATFORM_NO_OPENGL)

void dmNativeParseGLVersion( int *major, int *minor, int *rev )
{
    *major = 0;
    *minor = 0;
    *rev = 0;
}


void dmNativeRefreshContextParams( void )
{
    dmNativeWin.glMajor = 0;
    dmNativeWin.glMinor = 0;
    dmNativeWin.glRevision = 0;
    dmNativeWin.glProfile = 0;
    dmNativeWin.glForward = GL_FALSE;
}


void * dmNativeGetProcAddress( const char *procname )
{
    return NULL;
}


#else

static void _ClearGLError()
{
    GLint err = glGetError();
    while (err != 0)
    {
        err = glGetError();
    }
}

//========================================================================
// Parses the OpenGL version string and extracts the version number
//========================================================================

void dmNativeParseGLVersion( int *major, int *minor, int *rev )
{
    glGetIntegerv(GL_MAJOR_VERSION, major);
    glGetIntegerv(GL_MINOR_VERSION, minor);
    *rev = 0;

    GLint err = glGetError();
    if (err == 0) {
        return;
    }
    _ClearGLError();

    GLuint _major, _minor = 0, _rev = 0;
    const GLubyte *version;
    const GLubyte *ptr;

    // Get OpenGL version string
    version = glGetString( GL_VERSION );
    if( !version )
    {
        return;
    }

    // Parse string
    ptr = version;

    const char* opengles = "OpenGL ES ";
    if (strstr(ptr, opengles))
        ptr += strlen(opengles);

    for( _major = 0; *ptr >= '0' && *ptr <= '9'; ptr ++ )
    {
        _major = 10*_major + (*ptr - '0');
    }
    if( *ptr == '.' )
    {
        ptr ++;
        for( _minor = 0; *ptr >= '0' && *ptr <= '9'; ptr ++ )
        {
            _minor = 10*_minor + (*ptr - '0');
        }
        if( *ptr == '.' )
        {
            ptr ++;
            for( _rev = 0; *ptr >= '0' && *ptr <= '9'; ptr ++ )
            {
                _rev = 10*_rev + (*ptr - '0');
            }
        }
    }

    // Return parsed values
    *major = _major;
    *minor = _minor;
    *rev = _rev;
}

//========================================================================
// Reads back OpenGL context properties from the current context
//========================================================================

void dmNativeRefreshContextParams( void )
{
    dmNativeParseGLVersion( &dmNativeWin.glMajor, &dmNativeWin.glMinor,
                         &dmNativeWin.glRevision );

    dmNativeWin.glProfile = 0;
    dmNativeWin.glForward = GL_FALSE;

    // Read back the context profile, if applicable
    if( dmNativeWin.glMajor >= 3 )
    {
        GLint flags;
        glGetIntegerv( GL_CONTEXT_FLAGS, &flags );
        _ClearGLError();
        if( flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT )
        {
            dmNativeWin.glForward = GL_TRUE;
        }
    }

    if( dmNativeWin.glMajor > 3 ||
        ( dmNativeWin.glMajor == 3 && dmNativeWin.glMinor >= 2 ) )
    {
        GLint mask;
        glGetIntegerv( GL_CONTEXT_PROFILE_MASK, &mask );
        _ClearGLError();
        if( mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT )
        {
            dmNativeWin.glProfile = NATIVE_OPENGL_COMPAT_PROFILE;
        }
        else if( mask & GL_CONTEXT_CORE_PROFILE_BIT )
        {
            dmNativeWin.glProfile = NATIVE_OPENGL_CORE_PROFILE;
        }
    }
}


//************************************************************************
//****                    Native backend functions                         ****
//************************************************************************

//========================================================================
// Get the function pointer to an OpenGL function.  This function can be
// used to get access to extended OpenGL functions.
//========================================================================

void * dmNativeGetProcAddress( const char *procname )
{
    // Is Native initialized?
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return NULL;
    }

    return dmNativeOSGetProcAddress( procname );
}


#endif
