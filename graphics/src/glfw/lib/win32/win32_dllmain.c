//========================================================================
// GLFW - An OpenGL framework
// File:        win32_dllmain.c
// Platform:    Windows
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


#if defined(GLFW_BUILD_DLL)

//========================================================================
// DllMain()
//========================================================================

int WINAPI DllMain( HINSTANCE hinst, unsigned long reason, void *x )
{
    // NOTE: Some compilers complains about hinst and x never being used -
    // never mind that (we don't want to use them)!

    switch( reason )
    {
    case DLL_PROCESS_ATTACH:
        // Initializations
        //glfwInit();   // We don't want to do that now!
        break;
    case DLL_PROCESS_DETACH:
        // Do some cleanup
        glfwTerminate();
        break;
    };

    return 1;
}

#endif // GLFW_BUILD_DLL
