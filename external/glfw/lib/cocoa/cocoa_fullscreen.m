//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

//========================================================================
// Defold related changes;
//   Updated usage of CGDisplayMode* API usage from old deprecated calls.
//   modeIsGood(...) behaviour is more like GLFW 3+ (also due to removing
//   deprecated API calls).
//
// Note;
//   CGDisplayModeCopyPixelEncoding is deprecated in OSX 10.11+ but Apple
//   has not provided any alternative way of getting the bit depths, yet.
//   (No alternative prestented in the documentation at least, so we use
//   same solution as GLFW 3 for now.)
//========================================================================

//========================================================================
// Check whether the display mode should be included in enumeration
//========================================================================

static BOOL modeIsGood( CGDisplayModeRef mode )
{
    uint32_t flags = CGDisplayModeGetIOFlags(mode);
    if (!(flags & kDisplayModeValidFlag) || !(flags & kDisplayModeSafeFlag))
        return false;

    if (flags & kDisplayModeInterlacedFlag)
        return false;

    if (flags & kDisplayModeStretchedFlag)
        return false;

    CFStringRef format = CGDisplayModeCopyPixelEncoding(mode);
    if (CFStringCompare(format, CFSTR(IO16BitDirectPixels), 0) &&
        CFStringCompare(format, CFSTR(IO32BitDirectPixels), 0))
    {
        CFRelease(format);
        return false;
    }

    CFRelease(format);
    return true;
}

//========================================================================
// Convert Core Graphics display mode to GLFW video mode
//========================================================================

static GLFWvidmode vidmodeFromCGDisplayMode( CGDisplayModeRef mode )
{
    unsigned int width = CGDisplayModeGetWidth(mode);
    unsigned int height = CGDisplayModeGetHeight(mode);
    unsigned int bps = 8;

    CFStringRef format = CGDisplayModeCopyPixelEncoding(mode);
    if (CFStringCompare(format, CFSTR(IO16BitDirectPixels), 0) == 0) {
        bps = 16;
    } else if (CFStringCompare(format, CFSTR(IO32BitDirectPixels), 0) == 0) {
        bps = 32;
    }
    CFRelease(format);

    GLFWvidmode result;
    result.Width = width;
    result.Height = height;
    result.RedBits = bps;
    result.GreenBits = bps;
    result.BlueBits = bps;
    return result;
}


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Get a list of available video modes
//========================================================================

int _glfwPlatformGetVideoModes( GLFWvidmode *list, int maxcount )
{
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
    CFIndex n = CFArrayGetCount(modes);
    unsigned int j = 0;

    for(CFIndex i = 0; i < n && j < (unsigned)maxcount; i++ )
    {
        CGDisplayModeRef mode = (CGDisplayModeRef) CFArrayGetValueAtIndex(modes, i);
        if( modeIsGood( mode ) )
        {
            list[j++] = vidmodeFromCGDisplayMode( mode );
        }
    }
    CFRelease(modes);

    return j;
}

//========================================================================
// Get the desktop video mode
//========================================================================

void _glfwPlatformGetDesktopMode( GLFWvidmode *mode )
{
    *mode = vidmodeFromCGDisplayMode( _glfwLibrary.DesktopMode );
}

