//========================================================================
// GLFW - An OpenGL framework
// File:        macosx_window.c
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

#include "internal.h"

static _GLFWmacwindowfunctions _glfwMacFSWindowFunctions =
{
    _glfwMacFSOpenWindow,
    _glfwMacFSCloseWindow,
    _glfwMacFSSetWindowTitle,
    _glfwMacFSSetWindowSize,
    _glfwMacFSSetWindowPos,
    _glfwMacFSIconifyWindow,
    _glfwMacFSRestoreWindow,
    _glfwMacFSRefreshWindowParams,
    _glfwMacFSSetMouseCursorPos
};

static _GLFWmacwindowfunctions _glfwMacDWWindowFunctions =
{
    _glfwMacDWOpenWindow,
    _glfwMacDWCloseWindow,
    _glfwMacDWSetWindowTitle,
    _glfwMacDWSetWindowSize,
    _glfwMacDWSetWindowPos,
    _glfwMacDWIconifyWindow,
    _glfwMacDWRestoreWindow,
    _glfwMacDWRefreshWindowParams,
    _glfwMacDWSetMouseCursorPos
};

#define _glfwTestModifier( modifierMask, glfwKey ) \
if ( changed & modifierMask ) \
{ \
    _glfwInputKey( glfwKey, (modifiers & modifierMask ? GLFW_PRESS : GLFW_RELEASE) ); \
}

void _glfwHandleMacModifierChange( UInt32 modifiers )
{
    UInt32 changed = modifiers ^ _glfwInput.Modifiers;

    _glfwTestModifier( shiftKey,        GLFW_KEY_LSHIFT );
    _glfwTestModifier( rightShiftKey,   GLFW_KEY_RSHIFT );
    _glfwTestModifier( controlKey,      GLFW_KEY_LCTRL );
    _glfwTestModifier( rightControlKey, GLFW_KEY_RCTRL );
    _glfwTestModifier( optionKey,       GLFW_KEY_LALT );
    _glfwTestModifier( rightOptionKey,  GLFW_KEY_RALT );

    _glfwInput.Modifiers = modifiers;
}

void _glfwHandleMacKeyChange( UInt32 keyCode, int action )
{
    switch ( keyCode )
    {
        case MAC_KEY_ENTER:       _glfwInputKey( GLFW_KEY_ENTER,       action); break;
        case MAC_KEY_RETURN:      _glfwInputKey( GLFW_KEY_KP_ENTER,    action); break;
        case MAC_KEY_ESC:         _glfwInputKey( GLFW_KEY_ESC,         action); break;
        case MAC_KEY_F1:          _glfwInputKey( GLFW_KEY_F1,          action); break;
        case MAC_KEY_F2:          _glfwInputKey( GLFW_KEY_F2,          action); break;
        case MAC_KEY_F3:          _glfwInputKey( GLFW_KEY_F3,          action); break;
        case MAC_KEY_F4:          _glfwInputKey( GLFW_KEY_F4,          action); break;
        case MAC_KEY_F5:          _glfwInputKey( GLFW_KEY_F5,          action); break;
        case MAC_KEY_F6:          _glfwInputKey( GLFW_KEY_F6,          action); break;
        case MAC_KEY_F7:          _glfwInputKey( GLFW_KEY_F7,          action); break;
        case MAC_KEY_F8:          _glfwInputKey( GLFW_KEY_F8,          action); break;
        case MAC_KEY_F9:          _glfwInputKey( GLFW_KEY_F9,          action); break;
        case MAC_KEY_F10:         _glfwInputKey( GLFW_KEY_F10,         action); break;
        case MAC_KEY_F11:         _glfwInputKey( GLFW_KEY_F11,         action); break;
        case MAC_KEY_F12:         _glfwInputKey( GLFW_KEY_F12,         action); break;
        case MAC_KEY_F13:         _glfwInputKey( GLFW_KEY_F13,         action); break;
        case MAC_KEY_F14:         _glfwInputKey( GLFW_KEY_F14,         action); break;
        case MAC_KEY_F15:         _glfwInputKey( GLFW_KEY_F15,         action); break;
        case MAC_KEY_UP:          _glfwInputKey( GLFW_KEY_UP,          action); break;
        case MAC_KEY_DOWN:        _glfwInputKey( GLFW_KEY_DOWN,        action); break;
        case MAC_KEY_LEFT:        _glfwInputKey( GLFW_KEY_LEFT,        action); break;
        case MAC_KEY_RIGHT:       _glfwInputKey( GLFW_KEY_RIGHT,       action); break;
        case MAC_KEY_TAB:         _glfwInputKey( GLFW_KEY_TAB,         action); break;
        case MAC_KEY_BACKSPACE:   _glfwInputKey( GLFW_KEY_BACKSPACE,   action); break;
        case MAC_KEY_HELP:        _glfwInputKey( GLFW_KEY_INSERT,      action); break;
        case MAC_KEY_DEL:         _glfwInputKey( GLFW_KEY_DEL,         action); break;
        case MAC_KEY_PAGEUP:      _glfwInputKey( GLFW_KEY_PAGEUP,      action); break;
        case MAC_KEY_PAGEDOWN:    _glfwInputKey( GLFW_KEY_PAGEDOWN,    action); break;
        case MAC_KEY_HOME:        _glfwInputKey( GLFW_KEY_HOME,        action); break;
        case MAC_KEY_END:         _glfwInputKey( GLFW_KEY_END,         action); break;
        case MAC_KEY_KP_0:        _glfwInputKey( GLFW_KEY_KP_0,        action); break;
        case MAC_KEY_KP_1:        _glfwInputKey( GLFW_KEY_KP_1,        action); break;
        case MAC_KEY_KP_2:        _glfwInputKey( GLFW_KEY_KP_2,        action); break;
        case MAC_KEY_KP_3:        _glfwInputKey( GLFW_KEY_KP_3,        action); break;
        case MAC_KEY_KP_4:        _glfwInputKey( GLFW_KEY_KP_4,        action); break;
        case MAC_KEY_KP_5:        _glfwInputKey( GLFW_KEY_KP_5,        action); break;
        case MAC_KEY_KP_6:        _glfwInputKey( GLFW_KEY_KP_6,        action); break;
        case MAC_KEY_KP_7:        _glfwInputKey( GLFW_KEY_KP_7,        action); break;
        case MAC_KEY_KP_8:        _glfwInputKey( GLFW_KEY_KP_8,        action); break;
        case MAC_KEY_KP_9:        _glfwInputKey( GLFW_KEY_KP_9,        action); break;
        case MAC_KEY_KP_DIVIDE:   _glfwInputKey( GLFW_KEY_KP_DIVIDE,   action); break;
        case MAC_KEY_KP_MULTIPLY: _glfwInputKey( GLFW_KEY_KP_MULTIPLY, action); break;
        case MAC_KEY_KP_SUBTRACT: _glfwInputKey( GLFW_KEY_KP_SUBTRACT, action); break;
        case MAC_KEY_KP_ADD:      _glfwInputKey( GLFW_KEY_KP_ADD,      action); break;
        case MAC_KEY_KP_DECIMAL:  _glfwInputKey( GLFW_KEY_KP_DECIMAL,  action); break;
        case MAC_KEY_KP_EQUAL:    _glfwInputKey( GLFW_KEY_KP_EQUAL,    action); break;
        case MAC_KEY_KP_ENTER:    _glfwInputKey( GLFW_KEY_KP_ENTER,    action); break;
        default:
        {
            extern void *KCHRPtr;
            UInt32 state = 0;
            char charCode = (char)KeyTranslate( KCHRPtr, keyCode, &state );
            UppercaseText( &charCode, 1, smSystemScript );
            _glfwInputKey( (unsigned char)charCode, action );
        }
        break;
    }
}

EventTypeSpec GLFW_KEY_EVENT_TYPES[] =
{
    { kEventClassKeyboard, kEventRawKeyDown },
    { kEventClassKeyboard, kEventRawKeyUp },
    { kEventClassKeyboard, kEventRawKeyModifiersChanged }
};

OSStatus _glfwKeyEventHandler( EventHandlerCallRef handlerCallRef,
                               EventRef event,
                               void *userData )
{
    UInt32 keyCode;
    short int keyChar;
    UInt32 modifiers;

    switch( GetEventKind( event ) )
    {
        case kEventRawKeyDown:
        {
            if( GetEventParameter( event,
                                   kEventParamKeyCode,
                                   typeUInt32,
                                   NULL,
                                   sizeof( UInt32 ),
                                   NULL,
                                   &keyCode ) == noErr )
            {
                _glfwHandleMacKeyChange( keyCode, GLFW_PRESS );
            }
            if( GetEventParameter( event,
                                   kEventParamKeyUnicodes,
                                   typeUnicodeText,
                                   NULL,
                                   sizeof(keyChar),
                                   NULL,
                                   &keyChar) == noErr )
            {
                _glfwInputChar( keyChar, GLFW_PRESS );
            }
            return noErr;
        }

        case kEventRawKeyUp:
        {
            if( GetEventParameter( event,
                                   kEventParamKeyCode,
                                   typeUInt32,
                                   NULL,
                                   sizeof( UInt32 ),
                                   NULL,
                                   &keyCode ) == noErr )
            {
                _glfwHandleMacKeyChange( keyCode, GLFW_RELEASE );
            }
            if( GetEventParameter( event,
                                   kEventParamKeyUnicodes,
                                   typeUnicodeText,
                                   NULL,
                                   sizeof(keyChar),
                                   NULL,
                                   &keyChar) == noErr )
            {
                _glfwInputChar( keyChar, GLFW_RELEASE );
            }
            return noErr;
        }

        case kEventRawKeyModifiersChanged:
        {
            if( GetEventParameter( event,
                                   kEventParamKeyModifiers,
                                   typeUInt32,
                                   NULL,
                                   sizeof( UInt32 ),
                                   NULL,
                                   &modifiers ) == noErr )
            {
                _glfwHandleMacModifierChange( modifiers );
                return noErr;
            }
        }
        break;
    }

    return eventNotHandledErr;
}

EventTypeSpec GLFW_MOUSE_EVENT_TYPES[] =
{
    { kEventClassMouse, kEventMouseDown },
    { kEventClassMouse, kEventMouseUp },
    { kEventClassMouse, kEventMouseMoved },
    { kEventClassMouse, kEventMouseDragged },
    { kEventClassMouse, kEventMouseWheelMoved },
};

OSStatus _glfwMouseEventHandler( EventHandlerCallRef handlerCallRef,
                                 EventRef event,
                                 void *userData )
{
    switch( GetEventKind( event ) )
    {
        case kEventMouseDown:
        {
            WindowRef window;
            EventRecord oldStyleMacEvent;
            ConvertEventRefToEventRecord( event, &oldStyleMacEvent );
            if( FindWindow ( oldStyleMacEvent.where, &window ) == inMenuBar )
            {
                MenuSelect( oldStyleMacEvent.where );
                HiliteMenu(0);
                return noErr;
            }
            else
            {
                EventMouseButton button;
                if( GetEventParameter( event,
                                       kEventParamMouseButton,
                                       typeMouseButton,
                                       NULL,
                                       sizeof( EventMouseButton ),
                                       NULL,
                                       &button ) == noErr )
                {
                    button -= kEventMouseButtonPrimary;
                    if( button <= GLFW_MOUSE_BUTTON_LAST )
                    {
                        _glfwInputMouseClick( button
                                              + GLFW_MOUSE_BUTTON_LEFT,
                                              GLFW_PRESS );
                    }
                    return noErr;
                }
            }
            break;
        }

        case kEventMouseUp:
        {
            EventMouseButton button;
            if( GetEventParameter( event,
                                   kEventParamMouseButton,
                                   typeMouseButton,
                                   NULL,
                                   sizeof( EventMouseButton ),
                                   NULL,
                                   &button ) == noErr )
            {
                button -= kEventMouseButtonPrimary;
                if( button <= GLFW_MOUSE_BUTTON_LAST )
                {
                    _glfwInputMouseClick( button
                                          + GLFW_MOUSE_BUTTON_LEFT,
                                          GLFW_RELEASE );
                }
                return noErr;
            }
            break;
        }

        case kEventMouseMoved:
        case kEventMouseDragged:
        {
            HIPoint mouseLocation;
	    if( _glfwWin.MouseLock )
	    {
		if( GetEventParameter( event,
				       kEventParamMouseDelta,
				       typeHIPoint,
				       NULL,
				       sizeof( HIPoint ),
				       NULL,
				       &mouseLocation ) != noErr )
		{
		    break;
		}

		_glfwInput.MousePosX += mouseLocation.x;
		_glfwInput.MousePosY += mouseLocation.y;
	    }
	    else
	    {
		if( GetEventParameter( event,
				       kEventParamMouseLocation,
				       typeHIPoint,
				       NULL,
				       sizeof( HIPoint ),
				       NULL,
				       &mouseLocation ) != noErr )
		{
		    break;
		}

		_glfwInput.MousePosX = mouseLocation.x;
		_glfwInput.MousePosY = mouseLocation.y;

		if( !_glfwWin.Fullscreen )
		{
		    Rect content;
		    GetWindowBounds( _glfwWin.MacWindow,
		                     kWindowContentRgn,
				     &content );

		    _glfwInput.MousePosX -= content.left;
		    _glfwInput.MousePosY -= content.top;
		}
	    }

	    if( _glfwWin.MousePosCallback )
	    {
		_glfwWin.MousePosCallback( _glfwInput.MousePosX,
					   _glfwInput.MousePosY );
	    }

            break;
        }

        case kEventMouseWheelMoved:
        {
            EventMouseWheelAxis axis;
            if( GetEventParameter( event,
                                   kEventParamMouseWheelAxis,
                                   typeMouseWheelAxis,
                                   NULL,
                                   sizeof( EventMouseWheelAxis ),
                                   NULL,
                                   &axis) == noErr )
            {
                long wheelDelta;
                if( axis == kEventMouseWheelAxisY &&
                    GetEventParameter( event,
                                       kEventParamMouseWheelDelta,
                                       typeLongInteger,
                                       NULL,
                                       sizeof( long ),
                                       NULL,
                                       &wheelDelta ) == noErr )
                {
                    _glfwInput.WheelPos += wheelDelta;
                    if( _glfwWin.MouseWheelCallback )
                    {
                        _glfwWin.MouseWheelCallback( _glfwInput.WheelPos );
                    }
                    return noErr;
                }
            }
            break;
        }
    }

    return eventNotHandledErr;
}

EventTypeSpec GLFW_COMMAND_EVENT_TYPES[] =
{
    { kEventClassCommand, kEventCommandProcess }
};

OSStatus _glfwCommandHandler( EventHandlerCallRef handlerCallRef,
                                 EventRef event,
                                 void *userData )
{
    if( _glfwWin.SysKeysDisabled )
    {
        // TO DO: give adequate UI feedback that this is the case
        return eventNotHandledErr;
    }

    HICommand command;
    if( GetEventParameter( event,
                           kEventParamDirectObject,
                           typeHICommand,
                           NULL,
                           sizeof( HICommand ),
                           NULL,
                           &command ) == noErr )
    {
        switch( command.commandID )
        {
            case kHICommandClose:
            case kHICommandQuit:
            {
                // Check if the program wants us to close the window
                if( _glfwWin.WindowCloseCallback )
                {
                    if( _glfwWin.WindowCloseCallback() )
                    {
                        glfwCloseWindow();
                    }
                }
                else
                {
                    glfwCloseWindow();
                }
                return noErr;
            }
        }
    }

    return eventNotHandledErr;
}

EventTypeSpec GLFW_WINDOW_EVENT_TYPES[] =
{
  { kEventClassWindow, kEventWindowBoundsChanged },
  { kEventClassWindow, kEventWindowClose },
  { kEventClassWindow, kEventWindowDrawContent },
  { kEventClassWindow, kEventWindowActivated },
  { kEventClassWindow, kEventWindowDeactivated },
};

OSStatus _glfwWindowEventHandler( EventHandlerCallRef handlerCallRef,
                              EventRef event,
                              void *userData )
{
    switch( GetEventKind(event) )
    {
    	case kEventWindowBoundsChanged:
    	{
      	    WindowRef window;
      	    GetEventParameter( event, kEventParamDirectObject, typeWindowRef, NULL,
			       sizeof(WindowRef), NULL, &window );

      	    Rect rect;
      	    GetWindowPortBounds( window, &rect );

      	    if( _glfwWin.Width != rect.right ||
           	_glfwWin.Height != rect.bottom )
      	    {
        	aglUpdateContext(_glfwWin.AGLContext);

        	_glfwWin.Width  = rect.right;
        	_glfwWin.Height = rect.bottom;
        	if( _glfwWin.WindowSizeCallback )
        	{
          	    _glfwWin.WindowSizeCallback( _glfwWin.Width,
                                                _glfwWin.Height );
        	}
        	// Emulate (force) content invalidation
        	if( _glfwWin.WindowRefreshCallback )
        	{
                    _glfwWin.WindowRefreshCallback();
        	}
      	    }
      	    break;
    	}

    	case kEventWindowClose:
    	{
      	    // Check if the program wants us to close the window
      	    if( _glfwWin.WindowCloseCallback )
      	    {
          	if( _glfwWin.WindowCloseCallback() )
          	{
              	    glfwCloseWindow();
          	}
     	    }
      	    else
      	    {
          	glfwCloseWindow();
      	    }
      	    return noErr;
    	}

    	case kEventWindowDrawContent:
    	{
	    // Call user callback function
            if( _glfwWin.WindowRefreshCallback )
	    {
		_glfwWin.WindowRefreshCallback();
	    }
	    break;
        }

	case kEventWindowActivated:
	{
	    _glfwWin.Active = GL_TRUE;
	    break;
	}

	case kEventWindowDeactivated:
	{
	    _glfwWin.Active = GL_FALSE;
	    _glfwInputDeactivation();
	    break;
	}
    }

    return eventNotHandledErr;
}

int _glfwInstallEventHandlers( void )
{
    OSStatus error;

    _glfwWin.MouseUPP = NewEventHandlerUPP( _glfwMouseEventHandler );

    error = InstallEventHandler( GetApplicationEventTarget(),
                                 _glfwWin.MouseUPP,
                                 GetEventTypeCount( GLFW_MOUSE_EVENT_TYPES ),
                                 GLFW_MOUSE_EVENT_TYPES,
                                 NULL,
                                 NULL );
    if( error != noErr )
    {
        fprintf( stderr, "glfwOpenWindow failing because it can't install mouse event handler\n" );
        return GL_FALSE;
    }

    _glfwWin.CommandUPP = NewEventHandlerUPP( _glfwCommandHandler );

    error = InstallEventHandler( GetApplicationEventTarget(),
                                 _glfwWin.CommandUPP,
                                 GetEventTypeCount( GLFW_COMMAND_EVENT_TYPES ),
                                 GLFW_COMMAND_EVENT_TYPES,
                                 NULL,
                                 NULL );
    if( error != noErr )
    {
        fprintf( stderr, "glfwOpenWindow failing because it can't install command event handler\n" );
        return GL_FALSE;
    }

    _glfwWin.KeyboardUPP = NewEventHandlerUPP( _glfwKeyEventHandler );

    error = InstallEventHandler( GetApplicationEventTarget(),
                                 _glfwWin.KeyboardUPP,
                                 GetEventTypeCount( GLFW_KEY_EVENT_TYPES ),
                                 GLFW_KEY_EVENT_TYPES,
                                 NULL,
                                 NULL );
    if( error != noErr )
    {
        fprintf( stderr, "glfwOpenWindow failing because it can't install key event handler\n" );
        return GL_FALSE;
    }

    return GL_TRUE;
}

#define _setAGLAttribute( aglAttributeName, AGLparameter ) \
if ( AGLparameter != 0 ) \
{ \
    AGLpixelFormatAttributes[numAGLAttrs++] = aglAttributeName; \
    AGLpixelFormatAttributes[numAGLAttrs++] = AGLparameter; \
}

#define _getAGLAttribute( aglAttributeName, variableName ) \
{ \
    GLint aglValue; \
    (void)aglDescribePixelFormat( pixelFormat, aglAttributeName, &aglValue ); \
    variableName = aglValue; \
}

#define _setCGLAttribute( cglAttributeName, CGLparameter ) \
if ( CGLparameter != 0 ) \
{ \
    CGLpixelFormatAttributes[ numCGLAttrs++ ] = cglAttributeName; \
    CGLpixelFormatAttributes[ numCGLAttrs++ ] = CGLparameter; \
}

#define _getCGLAttribute( cglAttributeName, variableName ) \
{ \
    long cglValue; \
    (void)CGLDescribePixelFormat( CGLpfObj, 0, cglAttributeName, &cglValue ); \
    variableName = cglValue; \
}

int  _glfwPlatformOpenWindow( int width,
                              int height,
                              int redbits,
                              int greenbits,
                              int bluebits,
                              int alphabits,
                              int depthbits,
                              int stencilbits,
                              int mode,
                              _GLFWhints* hints )
{
    OSStatus error;
    ProcessSerialNumber psn;

    unsigned int windowAttributes;

    // TO DO: Refactor this function!
    _glfwWin.WindowFunctions = ( _glfwWin.Fullscreen ?
                               &_glfwMacFSWindowFunctions :
                               &_glfwMacDWWindowFunctions );

    // Windowed or fullscreen; AGL or CGL? Quite the mess...
    // AGL appears to be the only choice for attaching OpenGL contexts to
    // Carbon windows, but it leaves the user no control over fullscreen
    // mode stretching. Solution: AGL for windowed, CGL for fullscreen.
    if( !_glfwWin.Fullscreen )
    {
        // create AGL pixel format attribute list
        GLint AGLpixelFormatAttributes[256];
        int numAGLAttrs = 0;

        AGLpixelFormatAttributes[numAGLAttrs++] = AGL_RGBA;
        AGLpixelFormatAttributes[numAGLAttrs++] = AGL_DOUBLEBUFFER;

        if( hints->Stereo )
        {
            AGLpixelFormatAttributes[numAGLAttrs++] = AGL_STEREO;
        }

        _setAGLAttribute( AGL_AUX_BUFFERS,      hints->AuxBuffers);
        _setAGLAttribute( AGL_RED_SIZE,         redbits );
        _setAGLAttribute( AGL_GREEN_SIZE,       greenbits );
        _setAGLAttribute( AGL_BLUE_SIZE,        bluebits );
        _setAGLAttribute( AGL_ALPHA_SIZE,       alphabits );
        _setAGLAttribute( AGL_DEPTH_SIZE,       depthbits );
        _setAGLAttribute( AGL_STENCIL_SIZE,     stencilbits );
        _setAGLAttribute( AGL_ACCUM_RED_SIZE,   hints->AccumRedBits );
        _setAGLAttribute( AGL_ACCUM_GREEN_SIZE, hints->AccumGreenBits );
        _setAGLAttribute( AGL_ACCUM_BLUE_SIZE,  hints->AccumBlueBits );
        _setAGLAttribute( AGL_ACCUM_ALPHA_SIZE, hints->AccumAlphaBits );

	if( hints->Samples > 1 )
	{
	    _setAGLAttribute( AGL_SAMPLE_BUFFERS_ARB, 1 );
	    _setAGLAttribute( AGL_SAMPLES_ARB, hints->Samples );
	    AGLpixelFormatAttributes[numAGLAttrs++] = AGL_NO_RECOVERY;
	}

        AGLpixelFormatAttributes[numAGLAttrs++] = AGL_NONE;

        // create pixel format descriptor
        AGLDevice mainMonitor = GetMainDevice();
        AGLPixelFormat pixelFormat = aglChoosePixelFormat( &mainMonitor,
                                                           1,
                                                           AGLpixelFormatAttributes );
        if( pixelFormat == NULL )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't create a pixel format\n" );
            return GL_FALSE;
        }

        // store pixel format's values for _glfwPlatformGetWindowParam's use
        _getAGLAttribute( AGL_ACCELERATED,      _glfwWin.Accelerated );
        _getAGLAttribute( AGL_RED_SIZE,         _glfwWin.RedBits );
        _getAGLAttribute( AGL_GREEN_SIZE,       _glfwWin.GreenBits );
        _getAGLAttribute( AGL_BLUE_SIZE,        _glfwWin.BlueBits );
        _getAGLAttribute( AGL_ALPHA_SIZE,       _glfwWin.AlphaBits );
        _getAGLAttribute( AGL_DEPTH_SIZE,       _glfwWin.DepthBits );
        _getAGLAttribute( AGL_STENCIL_SIZE,     _glfwWin.StencilBits );
        _getAGLAttribute( AGL_ACCUM_RED_SIZE,   _glfwWin.AccumRedBits );
        _getAGLAttribute( AGL_ACCUM_GREEN_SIZE, _glfwWin.AccumGreenBits );
        _getAGLAttribute( AGL_ACCUM_BLUE_SIZE,  _glfwWin.AccumBlueBits );
        _getAGLAttribute( AGL_ACCUM_ALPHA_SIZE, _glfwWin.AccumAlphaBits );
        _getAGLAttribute( AGL_AUX_BUFFERS,      _glfwWin.AuxBuffers );
        _getAGLAttribute( AGL_STEREO,           _glfwWin.Stereo );
        _getAGLAttribute( AGL_SAMPLES_ARB,      _glfwWin.Samples );
        _glfwWin.RefreshRate = hints->RefreshRate;

        // create AGL context
        _glfwWin.AGLContext = aglCreateContext( pixelFormat, NULL );

        aglDestroyPixelFormat( pixelFormat );

        if( _glfwWin.AGLContext == NULL )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't create an OpenGL context\n" );
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        if (_glfwLibrary.Unbundled)
        {
            if( GetCurrentProcess( &psn ) != noErr )
            {
                fprintf( stderr, "glfwOpenWindow failing because it can't get its PSN\n" );
                _glfwPlatformCloseWindow();
                return GL_FALSE;
            }

    	    if( TransformProcessType( &psn, kProcessTransformToForegroundApplication ) != noErr )
            {
                fprintf( stderr, "glfwOpenWindow failing because it can't become a foreground application\n" );
                _glfwPlatformCloseWindow();
                return GL_FALSE;
            }
            
            /* Keith Bauer 2007-07-12 - I don't believe this is desirable
    	    if( SetFrontProcess( &psn ) != noErr )
            {
                fprintf( stderr, "glfwOpenWindow failing because it can't become the front process\n" );
                _glfwPlatformCloseWindow();
                return GL_FALSE;
            }
            */
        }
	    
        // create window
        Rect windowContentBounds;
        windowContentBounds.left = 0;
        windowContentBounds.top = 0;
        windowContentBounds.right = width;
        windowContentBounds.bottom = height;

        windowAttributes = ( kWindowCloseBoxAttribute        \
                           | kWindowCollapseBoxAttribute     \
                           | kWindowStandardHandlerAttribute );

        if( hints->WindowNoResize )
        {
            windowAttributes |= kWindowLiveResizeAttribute;
        }
        else
        {
            windowAttributes |= ( kWindowFullZoomAttribute | kWindowResizableAttribute );
        }

        error = CreateNewWindow( kDocumentWindowClass,
                                 windowAttributes,
                                 &windowContentBounds,
                                 &( _glfwWin.MacWindow ) );
        if( ( error != noErr ) || ( _glfwWin.MacWindow == NULL ) )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't create a window\n" );
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        _glfwWin.WindowUPP = NewEventHandlerUPP( _glfwWindowEventHandler );

        error = InstallWindowEventHandler( _glfwWin.MacWindow,
                                           _glfwWin.WindowUPP,
                                           GetEventTypeCount( GLFW_WINDOW_EVENT_TYPES ),
                                           GLFW_WINDOW_EVENT_TYPES,
                                           NULL,
                                           NULL );
        if( error != noErr )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't install window event handlers\n" );
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        // Don't care if we fail here
        (void)SetWindowTitleWithCFString( _glfwWin.MacWindow, CFSTR( "GLFW Window" ) );
        (void)RepositionWindow( _glfwWin.MacWindow,
                                NULL,
                                kWindowCenterOnMainScreen );

        if( !aglSetDrawable( _glfwWin.AGLContext,
                             GetWindowPort( _glfwWin.MacWindow ) ) )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't draw to the window\n" );
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        // Make OpenGL context current
        if( !aglSetCurrentContext( _glfwWin.AGLContext ) )
        {
            fprintf( stderr, "glfwOpenWindow failing because it can't make the OpenGL context current\n" );
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        // show window
        ShowWindow( _glfwWin.MacWindow );

        return GL_TRUE;
    }
    else
    {
        CGDisplayErr cgErr;
        CGLError cglErr;

        CFDictionaryRef optimalMode;

        CGLPixelFormatObj CGLpfObj;
        long numCGLvs = 0;

        CGLPixelFormatAttribute CGLpixelFormatAttributes[64];
        int numCGLAttrs = 0;

        // variables for enumerating color depths
        long rgbColorDepth;
        long rgbaAccumDepth = 0;
        int rgbChannelDepth = 0;

        // CGL pixel format attributes
        _setCGLAttribute( kCGLPFADisplayMask,
                          CGDisplayIDToOpenGLDisplayMask( kCGDirectMainDisplay ) );

        if( hints->Stereo )
        {
            CGLpixelFormatAttributes[ numCGLAttrs++ ] = kCGLPFAStereo;
        }

	if( hints->Samples > 1 )
	{
	    _setCGLAttribute( kCGLPFASamples,       (CGLPixelFormatAttribute)hints->Samples );
	    _setCGLAttribute( kCGLPFASampleBuffers, (CGLPixelFormatAttribute)1 );
	    CGLpixelFormatAttributes[ numCGLAttrs++ ] = kCGLPFANoRecovery;
	}

        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = kCGLPFAFullScreen;
        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = kCGLPFADoubleBuffer;
        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = kCGLPFAAccelerated;
        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = kCGLPFANoRecovery;
        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = kCGLPFAMinimumPolicy;

        _setCGLAttribute( kCGLPFAAccumSize,
                          (CGLPixelFormatAttribute)( hints->AccumRedBits \
                                                   + hints->AccumGreenBits \
                                                   + hints->AccumBlueBits \
                                                   + hints->AccumAlphaBits ) );

        _setCGLAttribute( kCGLPFAAlphaSize,   (CGLPixelFormatAttribute)alphabits );
        _setCGLAttribute( kCGLPFADepthSize,   (CGLPixelFormatAttribute)depthbits );
        _setCGLAttribute( kCGLPFAStencilSize, (CGLPixelFormatAttribute)stencilbits );
        _setCGLAttribute( kCGLPFAAuxBuffers,  (CGLPixelFormatAttribute)hints->AuxBuffers );

        CGLpixelFormatAttributes[ numCGLAttrs++ ]     = (CGLPixelFormatAttribute)NULL;

        // create a suitable pixel format with above attributes..
        cglErr = CGLChoosePixelFormat( CGLpixelFormatAttributes,
                                       &CGLpfObj,
                                       &numCGLvs );
        if( cglErr != kCGLNoError )
	{
	    return GL_FALSE;
	}

        // ..and create a rendering context using that pixel format
        cglErr = CGLCreateContext( CGLpfObj, NULL, &_glfwWin.CGLContext );
        if( cglErr != kCGLNoError )
	{
	    return GL_FALSE;
	}

        // enumerate depth of RGB channels - unlike AGL, CGL works with
        // a single parameter reflecting the full depth of the frame buffer
        (void)CGLDescribePixelFormat( CGLpfObj, 0, kCGLPFAColorSize, &rgbColorDepth );
        if( rgbColorDepth == 24 || rgbColorDepth == 32 )
	{
	    rgbChannelDepth = 8;
	}
        if( rgbColorDepth == 16 )
	{
	    rgbChannelDepth = 5;
	}

        // get pixel depth of accumulator - I haven't got the slightest idea
        // how this number conforms to any other channel depth than 8 bits,
        // so this might end up giving completely knackered results...
        (void)CGLDescribePixelFormat( CGLpfObj, 0, kCGLPFAAccumSize, &rgbaAccumDepth );
        if( rgbaAccumDepth == 32 )
	{
	    rgbaAccumDepth = 8;
	}

        // store values of pixel format for _glfwPlatformGetWindowParam's use
        _getCGLAttribute( kCGLPFAAccelerated, _glfwWin.Accelerated );
        _getCGLAttribute( rgbChannelDepth,    _glfwWin.RedBits );
        _getCGLAttribute( rgbChannelDepth,    _glfwWin.GreenBits );
        _getCGLAttribute( rgbChannelDepth,    _glfwWin.BlueBits );
        _getCGLAttribute( kCGLPFAAlphaSize,   _glfwWin.AlphaBits );
        _getCGLAttribute( kCGLPFADepthSize,   _glfwWin.DepthBits );
        _getCGLAttribute( kCGLPFAStencilSize, _glfwWin.StencilBits );
        _getCGLAttribute( rgbaAccumDepth,     _glfwWin.AccumRedBits );
        _getCGLAttribute( rgbaAccumDepth,     _glfwWin.AccumGreenBits );
        _getCGLAttribute( rgbaAccumDepth,     _glfwWin.AccumBlueBits );
        _getCGLAttribute( rgbaAccumDepth,     _glfwWin.AccumAlphaBits );
        _getCGLAttribute( kCGLPFAAuxBuffers,  _glfwWin.AuxBuffers );
        _getCGLAttribute( kCGLPFAStereo,      _glfwWin.Stereo );
        _glfwWin.RefreshRate = hints->RefreshRate;

        // destroy our pixel format
        (void)CGLDestroyPixelFormat( CGLpfObj );

        // capture the display for our application
        cgErr = CGCaptureAllDisplays();
        if( cgErr != kCGErrorSuccess )
	{
	    return GL_FALSE;
	}

        // find closest matching NON-STRETCHED display mode..
        optimalMode = CGDisplayBestModeForParametersAndRefreshRateWithProperty( kCGDirectMainDisplay,
	                                                                            rgbColorDepth,
	                                                                            width,
	/* Check further to the right -> */                                         height,
	                                                                            hints->RefreshRate,
	                                                                            NULL,
	                                                                            NULL );
        if( optimalMode == NULL )
	{
	    return GL_FALSE;
	}

        // ..and switch to that mode
        cgErr = CGDisplaySwitchToMode( kCGDirectMainDisplay, optimalMode );
        if( cgErr != kCGErrorSuccess )
	{
	    return GL_FALSE;
	}

        // switch to our OpenGL context, and bring it up fullscreen
        cglErr = CGLSetCurrentContext( _glfwWin.CGLContext );
        if( cglErr != kCGLNoError )
	{
	    return GL_FALSE;
	}

        cglErr = CGLSetFullScreen( _glfwWin.CGLContext );
        if( cglErr != kCGLNoError )
	{
	    return GL_FALSE;
	}

        return GL_TRUE;
    }
}

void _glfwPlatformCloseWindow( void )
{
    if( _glfwWin.WindowFunctions != NULL )
    {
        if( _glfwWin.WindowUPP != NULL )
        {
            DisposeEventHandlerUPP( _glfwWin.WindowUPP );
            _glfwWin.WindowUPP = NULL;
        }

        _glfwWin.WindowFunctions->CloseWindow();

        if( !_glfwWin.Fullscreen && _glfwWin.AGLContext != NULL )
        {
            aglSetCurrentContext( NULL );
            aglSetDrawable( _glfwWin.AGLContext, NULL );
            aglDestroyContext( _glfwWin.AGLContext );
            _glfwWin.AGLContext = NULL;
        }

        if( _glfwWin.Fullscreen && _glfwWin.CGLContext != NULL )
        {
            CGLSetCurrentContext( NULL );
            CGLClearDrawable( _glfwWin.CGLContext );
            CGLDestroyContext( _glfwWin.CGLContext );
            CGReleaseAllDisplays();
            _glfwWin.CGLContext = NULL;
        }

        if( _glfwWin.MacWindow != NULL )
        {
            ReleaseWindow( _glfwWin.MacWindow );
            _glfwWin.MacWindow = NULL;
        }

        _glfwWin.WindowFunctions = NULL;
    }
}

void _glfwPlatformSetWindowTitle( const char *title )
{
    _glfwWin.WindowFunctions->SetWindowTitle( title );
}

void _glfwPlatformSetWindowSize( int width, int height )
{
    _glfwWin.WindowFunctions->SetWindowSize( width, height );
}

void _glfwPlatformSetWindowPos( int x, int y )
{
    _glfwWin.WindowFunctions->SetWindowPos( x, y );
}

void _glfwPlatformIconifyWindow( void )
{
    _glfwWin.WindowFunctions->IconifyWindow();
}

void _glfwPlatformRestoreWindow( void )
{
    _glfwWin.WindowFunctions->RestoreWindow();
}

void _glfwPlatformSwapBuffers( void )
{
    if( !_glfwWin.Fullscreen )
    {
        aglSwapBuffers( _glfwWin.AGLContext );
    }
    else
    {
        CGLFlushDrawable( _glfwWin.CGLContext );
    }
}

void _glfwPlatformSwapInterval( int interval )
{
    GLint AGLparameter = interval;

    // CGL doesn't seem to like intervals other than 0 (vsync off) or 1 (vsync on)
    long CGLparameter = ( interval == 0 ? 0 : 1 );

    if( !_glfwWin.Fullscreen )
    {
        // Don't care if we fail here..
        (void)aglSetInteger( _glfwWin.AGLContext,
                             AGL_SWAP_INTERVAL,
                             &AGLparameter );
    }
    else
    {
        // ..or here
        (void)CGLSetParameter( _glfwWin.CGLContext,
                               kCGLCPSwapInterval,
                               &CGLparameter );
    }
}

void _glfwPlatformRefreshWindowParams( void )
{
    _glfwWin.WindowFunctions->RefreshWindowParams();
}

int _glfwPlatformGetWindowParam( int param )
{
    switch ( param )
    {
        case GLFW_ACCELERATED:      return _glfwWin.Accelerated;    break;
        case GLFW_RED_BITS:         return _glfwWin.RedBits;        break;
        case GLFW_GREEN_BITS:       return _glfwWin.GreenBits;      break;
        case GLFW_BLUE_BITS:        return _glfwWin.BlueBits;       break;
        case GLFW_ALPHA_BITS:       return _glfwWin.AlphaBits;      break;
        case GLFW_DEPTH_BITS:       return _glfwWin.DepthBits;      break;
        case GLFW_STENCIL_BITS:     return _glfwWin.StencilBits;    break;
        case GLFW_ACCUM_RED_BITS:   return _glfwWin.AccumRedBits;   break;
        case GLFW_ACCUM_GREEN_BITS: return _glfwWin.AccumGreenBits; break;
        case GLFW_ACCUM_BLUE_BITS:  return _glfwWin.AccumBlueBits;  break;
        case GLFW_ACCUM_ALPHA_BITS: return _glfwWin.AccumAlphaBits; break;
        case GLFW_AUX_BUFFERS:      return _glfwWin.AuxBuffers;     break;
        case GLFW_STEREO:           return _glfwWin.Stereo;         break;
        case GLFW_REFRESH_RATE:     return _glfwWin.RefreshRate;    break;
        default:                    return GL_FALSE;
    }
}

void _glfwPlatformPollEvents( void )
{
    EventRef event;
    EventTargetRef eventDispatcher = GetEventDispatcherTarget();

    while ( ReceiveNextEvent( 0, NULL, 0.0, TRUE, &event ) == noErr )
    {
        SendEventToEventTarget( event, eventDispatcher );
        ReleaseEvent( event );
    }
}

void _glfwPlatformWaitEvents( void )
{
    EventRef event;

    // Wait for new events
    ReceiveNextEvent( 0, NULL, kEventDurationForever, FALSE, &event );

    // Poll new events
    _glfwPlatformPollEvents();
}

void _glfwPlatformHideMouseCursor( void )
{
    // TO DO: What if we fail here?
    CGDisplayHideCursor( kCGDirectMainDisplay );
    CGAssociateMouseAndMouseCursorPosition( false );
}

void _glfwPlatformShowMouseCursor( void )
{
    // TO DO: What if we fail here?
    CGDisplayShowCursor( kCGDirectMainDisplay );
    CGAssociateMouseAndMouseCursorPosition( true );
}

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
    _glfwWin.WindowFunctions->SetMouseCursorPos( x, y );
}

int _glfwMacFSOpenWindow( int width,
                          int height,
                          int redbits,
                          int greenbits,
                          int bluebits,
                          int alphabits,
                          int depthbits,
                          int stencilbits,
                          int accumredbits,
                          int accumgreenbits,
                          int accumbluebits,
                          int accumalphabits,
                          int auxbuffers,
                          int stereo,
                          int refreshrate )
{
    return GL_FALSE;
}

void _glfwMacFSCloseWindow( void )
{
    // TO DO: un-capture displays, &c.
}

void _glfwMacFSSetWindowTitle( const char *title )
{
    // no-op really, change "fake" mini-window title
    _glfwMacDWSetWindowTitle( title );
}

void _glfwMacFSSetWindowSize( int width, int height )
{
    // TO DO: something funky for full-screen
    _glfwMacDWSetWindowSize( width, height );
}

void _glfwMacFSSetWindowPos( int x, int y )
{
    // no-op really, change "fake" mini-window position
    _glfwMacDWSetWindowPos( x, y );
}

void _glfwMacFSIconifyWindow( void )
{
    // TO DO: Something funky for full-screen
    _glfwMacDWIconifyWindow();
}

void _glfwMacFSRestoreWindow( void )
{
    _glfwMacDWRestoreWindow();
    // TO DO: Something funky for full-screen
}

void _glfwMacFSRefreshWindowParams( void )
{
    // TO DO: implement this!
}

void _glfwMacFSSetMouseCursorPos( int x, int y )
{
    // TO DO: what if we fail here?
    CGDisplayMoveCursorToPoint( kCGDirectMainDisplay,
                                CGPointMake( x, y ) );
}

int  _glfwMacDWOpenWindow( int width,
                           int height,
                           int redbits,
                           int greenbits,
                           int bluebits,
                           int alphabits,
                           int depthbits,
                           int stencilbits,
                           int accumredbits,
                           int accumgreenbits,
                           int accumbluebits,
                           int accumalphabits,
                           int auxbuffers,
                           int stereo,
                           int refreshrate )
{
    return GL_FALSE;
}

void _glfwMacDWCloseWindow( void )
{
}

void _glfwMacDWSetWindowTitle( const char *title )
{
    CFStringRef windowTitle = CFStringCreateWithCString( kCFAllocatorDefault,
                                                         title,
                                                         kCFStringEncodingISOLatin1 );

    // Don't care if we fail
    (void)SetWindowTitleWithCFString( _glfwWin.MacWindow, windowTitle );

    CFRelease( windowTitle );
}

void _glfwMacDWSetWindowSize( int width, int height )
{
    SizeWindow( _glfwWin.MacWindow,
                width,
                height,
                TRUE );
}

void _glfwMacDWSetWindowPos( int x, int y )
{
    // TO DO: take main monitor bounds into account
    MoveWindow( _glfwWin.MacWindow,
                x,
                y,
                FALSE );
}

void _glfwMacDWIconifyWindow( void )
{
    // TO DO: What if we fail here?
    (void)CollapseWindow( _glfwWin.MacWindow,
                          TRUE );
}

void _glfwMacDWRestoreWindow( void )
{
    // TO DO: What if we fail here?
    (void)CollapseWindow( _glfwWin.MacWindow,
                          FALSE );
}

void _glfwMacDWRefreshWindowParams( void )
{
    // TO DO: implement this!
}

void _glfwMacDWSetMouseCursorPos( int x, int y )
{
    Rect content;
    GetWindowBounds(_glfwWin.MacWindow, kWindowContentRgn, &content);

    _glfwInput.MousePosX = x + content.left;
    _glfwInput.MousePosY = y + content.top;

    CGDisplayMoveCursorToPoint( kCGDirectMainDisplay,
			        CGPointMake( _glfwInput.MousePosX,
					     _glfwInput.MousePosY ) );
}

