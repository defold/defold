//========================================================================
// GLFW - An OpenGL framework
// File:        win32_enable.c
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


//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// _glfwLLKeyboardProc() - Low level keyboard callback function (used to
// disable system keys under Windows NT).
//========================================================================

LRESULT CALLBACK _glfwLLKeyboardProc( int nCode, WPARAM wParam,
    LPARAM lParam )
{
    BOOL syskeys = 0;
    PKBDLLHOOKSTRUCT p;

    // We are only looking for keyboard events - interpret lParam as a
    // pointer to a KBDLLHOOKSTRUCT
    p = (PKBDLLHOOKSTRUCT) lParam;

    // If nCode == HC_ACTION, then we have a keyboard event
    if( nCode == HC_ACTION )
    {
        switch( wParam )
        {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
                // Detect: ALT+TAB, ALT+ESC, ALT+F4, CTRL+ESC,
                // LWIN, RWIN, APPS (mysterious menu key)
                syskeys = ( p->vkCode == VK_TAB &&
                            p->flags & LLKHF_ALTDOWN ) ||
                          ( p->vkCode == VK_ESCAPE &&
                            p->flags & LLKHF_ALTDOWN ) ||
                          ( p->vkCode == VK_F4 &&
                            p->flags & LLKHF_ALTDOWN ) ||
                          ( p->vkCode == VK_ESCAPE &&
                            (GetKeyState(VK_CONTROL) & 0x8000)) ||
                          p->vkCode == VK_LWIN ||
                          p->vkCode == VK_RWIN ||
                          p->vkCode == VK_APPS;
                break;

            default:
                break;
        }
    }

    // Was it a system key combination (e.g. ALT+TAB)?
    if( syskeys )
    {
        // Pass the key event to our window message loop
        if( _glfwWin.Opened )
        {
            PostMessage( _glfwWin.Wnd, (UINT) wParam, p->vkCode, 0 );
        }

        // We've taken care of it - don't let the system know about this
        // key event
        return 1;
    }
    else
    {
        // It's a harmless key press, let the system deal with it
        return CallNextHookEx( _glfwWin.KeyboardHook, nCode, wParam,
                               lParam );
    }
}



//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// _glfwPlatformEnableSystemKeys() - Enable system keys
// _glfwPlatformDisableSystemKeys() - Disable system keys
//========================================================================

void _glfwPlatformEnableSystemKeys( void )
{
    BOOL bOld;

    // Use different methods depending on operating system version
    if( _glfwLibrary.Sys.WinVer >= _GLFW_WIN_NT4 )
    {
        if( _glfwWin.KeyboardHook != NULL )
        {
            UnhookWindowsHookEx( _glfwWin.KeyboardHook );
            _glfwWin.KeyboardHook = NULL;
        }
    }
    else
    {
        (void) SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, FALSE,
                                     &bOld, 0 );
    }
}

void _glfwPlatformDisableSystemKeys( void )
{
    BOOL bOld;

    // Use different methods depending on operating system version
    if( _glfwLibrary.Sys.WinVer >= _GLFW_WIN_NT4 )
    {
        // Under Windows NT, install a low level keyboard hook
        _glfwWin.KeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,
                                    _glfwLLKeyboardProc,
                                    _glfwLibrary.Instance,
                                    0 );
    }
    else
    {
        // Under Windows 95/98/ME, fool Windows that a screensaver
        // is running => prevents ALT+TAB, CTRL+ESC and CTRL+ALT+DEL
        (void) SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, TRUE,
                                     &bOld, 0 );
    }
}

