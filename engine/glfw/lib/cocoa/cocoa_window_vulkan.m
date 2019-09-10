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


#import <CoreVideo/CVDisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>
#include "internal.h"

//========================================================================
// Delegate for window related notifications
// (but also used as an application delegate)
//========================================================================

@interface GLFWWindowDelegate : NSObject
@end

@implementation GLFWWindowDelegate

- (void)queueRender
{
    _glfwWin.countDown--;
}

- (BOOL)windowShouldClose:(id)window
{
    if( _glfwWin.windowCloseCallback )
    {
        if( !_glfwWin.windowCloseCallback() )
        {
            return NO;
        }
    }

    // This is horribly ugly, but it works
    glfwCloseWindow();
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification
{
    [_glfwWin.context update];

    NSRect contentRect =
        [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];

    contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];
    _glfwWin.width = contentRect.size.width;
    _glfwWin.height = contentRect.size.height;

    if( _glfwWin.windowSizeCallback )
    {
        _glfwWin.windowSizeCallback( _glfwWin.width, _glfwWin.height );
    }
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    _glfwWin.iconified = GL_TRUE;
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    _glfwWin.iconified = GL_FALSE;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    _glfwWin.active = GL_TRUE;
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(1);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    _glfwWin.active = GL_FALSE;
    _glfwInputDeactivation();
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(0);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    if( _glfwWin.windowCloseCallback )
    {
        if( !_glfwWin.windowCloseCallback() )
        {
            return NSTerminateCancel;
        }
    }

    // This is horribly ugly, but it works
    glfwCloseWindow();
    return NSTerminateCancel;
}

@end

// TODO: Need to find mappings for F13-F15, volume down/up/mute, and eject.
static const unsigned int MAC_TO_GLFW_KEYCODE_MAPPING[128] =
{
    /* 00 */ 'A',
    /* 01 */ 'S',
    /* 02 */ 'D',
    /* 03 */ 'F',
    /* 04 */ 'H',
    /* 05 */ 'G',
    /* 06 */ 'Z',
    /* 07 */ 'X',
    /* 08 */ 'C',
    /* 09 */ 'V',
    /* 0a */ -1,
    /* 0b */ 'B',
    /* 0c */ 'Q',
    /* 0d */ 'W',
    /* 0e */ 'E',
    /* 0f */ 'R',
    /* 10 */ 'Y',
    /* 11 */ 'T',
    /* 12 */ '1',
    /* 13 */ '2',
    /* 14 */ '3',
    /* 15 */ '4',
    /* 16 */ '6',
    /* 17 */ '5',
    /* 18 */ '=',
    /* 19 */ '9',
    /* 1a */ '7',
    /* 1b */ '-',
    /* 1c */ '8',
    /* 1d */ '0',
    /* 1e */ ']',
    /* 1f */ 'O',
    /* 20 */ 'U',
    /* 21 */ '[',
    /* 22 */ 'I',
    /* 23 */ 'P',
    /* 24 */ GLFW_KEY_ENTER,
    /* 25 */ 'L',
    /* 26 */ 'J',
    /* 27 */ '\'',
    /* 28 */ 'K',
    /* 29 */ ';',
    /* 2a */ '\\',
    /* 2b */ ',',
    /* 2c */ '/',
    /* 2d */ 'N',
    /* 2e */ 'M',
    /* 2f */ '.',
    /* 30 */ GLFW_KEY_TAB,
    /* 31 */ GLFW_KEY_SPACE,
    /* 32 */ '`',
    /* 33 */ GLFW_KEY_BACKSPACE,
    /* 34 */ -1,
    /* 35 */ GLFW_KEY_ESC,
    /* 36 */ GLFW_KEY_RSUPER,
    /* 37 */ GLFW_KEY_LSUPER,
    /* 38 */ GLFW_KEY_LSHIFT,
    /* 39 */ GLFW_KEY_CAPS_LOCK,
    /* 3a */ GLFW_KEY_LALT,
    /* 3b */ GLFW_KEY_LCTRL,
    /* 3c */ GLFW_KEY_RSHIFT,
    /* 3d */ GLFW_KEY_RALT,
    /* 3e */ GLFW_KEY_RCTRL,
    /* 3f */ -1, /*Function*/
    /* 40 */ GLFW_KEY_F17,
    /* 41 */ GLFW_KEY_KP_DECIMAL,
    /* 42 */ -1,
    /* 43 */ GLFW_KEY_KP_MULTIPLY,
    /* 44 */ -1,
    /* 45 */ GLFW_KEY_KP_ADD,
    /* 46 */ -1,
    /* 47 */ -1, /*KeypadClear*/
    /* 48 */ -1, /*VolumeUp*/
    /* 49 */ -1, /*VolumeDown*/
    /* 4a */ -1, /*Mute*/
    /* 4b */ GLFW_KEY_KP_DIVIDE,
    /* 4c */ GLFW_KEY_KP_ENTER,
    /* 4d */ -1,
    /* 4e */ GLFW_KEY_KP_SUBTRACT,
    /* 4f */ GLFW_KEY_F18,
    /* 50 */ GLFW_KEY_F19,
    /* 51 */ GLFW_KEY_KP_EQUAL,
    /* 52 */ GLFW_KEY_KP_0,
    /* 53 */ GLFW_KEY_KP_1,
    /* 54 */ GLFW_KEY_KP_2,
    /* 55 */ GLFW_KEY_KP_3,
    /* 56 */ GLFW_KEY_KP_4,
    /* 57 */ GLFW_KEY_KP_5,
    /* 58 */ GLFW_KEY_KP_6,
    /* 59 */ GLFW_KEY_KP_7,
    /* 5a */ GLFW_KEY_F20,
    /* 5b */ GLFW_KEY_KP_8,
    /* 5c */ GLFW_KEY_KP_9,
    /* 5d */ -1,
    /* 5e */ -1,
    /* 5f */ -1,
    /* 60 */ GLFW_KEY_F5,
    /* 61 */ GLFW_KEY_F6,
    /* 62 */ GLFW_KEY_F7,
    /* 63 */ GLFW_KEY_F3,
    /* 64 */ GLFW_KEY_F8,
    /* 65 */ GLFW_KEY_F9,
    /* 66 */ -1,
    /* 67 */ GLFW_KEY_F11,
    /* 68 */ -1,
    /* 69 */ GLFW_KEY_F13,
    /* 6a */ GLFW_KEY_F16,
    /* 6b */ GLFW_KEY_F14,
    /* 6c */ -1,
    /* 6d */ GLFW_KEY_F10,
    /* 6e */ -1,
    /* 6f */ GLFW_KEY_F12,
    /* 70 */ -1,
    /* 71 */ GLFW_KEY_F15,
    /* 72 */ GLFW_KEY_INSERT, /*Help*/
    /* 73 */ GLFW_KEY_HOME,
    /* 74 */ GLFW_KEY_PAGEUP,
    /* 75 */ GLFW_KEY_DEL,
    /* 76 */ GLFW_KEY_F4,
    /* 77 */ GLFW_KEY_END,
    /* 78 */ GLFW_KEY_F2,
    /* 79 */ GLFW_KEY_PAGEDOWN,
    /* 7a */ GLFW_KEY_F1,
    /* 7b */ GLFW_KEY_LEFT,
    /* 7c */ GLFW_KEY_RIGHT,
    /* 7d */ GLFW_KEY_DOWN,
    /* 7e */ GLFW_KEY_UP,
    /* 7f */ -1,
};

int _glfwPlatformGetWindowRefreshRate( void )
{
    CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod((CVDisplayLinkRef)_glfwWin.displayLink);
    float refresh_rate = (float)time.timeScale / (float)time.timeValue;
    return (refresh_rate + 0.5f);
}

//========================================================================
// Converts a Mac OS X keycode to a GLFW keycode
//========================================================================

static int convertMacKeyCode( unsigned int macKeyCode )
{
    if( macKeyCode >= 128 )
    {
        return -1;
    }

    // This treats keycodes as *positional*; that is, we'll return 'a'
    // for the key left of 's', even on an AZERTY keyboard.  The charInput
    // function should still get 'q' though.
    return MAC_TO_GLFW_KEYCODE_MAPPING[macKeyCode];
}

//========================================================================
// Content view class for the GLFW window
//========================================================================

@interface GLFWContentView : NSView
@end

@implementation GLFWContentView

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

-(CALayer*) makeBackingLayer {
    CALayer* layer      = [self.class.layerClass layer];
    CGSize viewScale    = [self convertSizeToBacking: CGSizeMake(1.0, 1.0)];
    layer.contentsScale = MIN(viewScale.width, viewScale.height);
    return layer;
}

- (void)mouseDown:(NSEvent *)event
{
    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event
{
    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
}

- (void)mouseMoved:(NSEvent *)event
{
    if( _glfwWin.mouseLock )
    {
        _glfwInput.MousePosX += [event deltaX];
        _glfwInput.MousePosY += [event deltaY];
    }
    else
    {
        // Cocoa coordinate system has origin at lower left
        NSPoint p = [event locationInWindow];
        p.y = [[_glfwWin.window contentView] bounds].size.height - p.y;

        // Need convert mouse coord into backing coords
        // which will be different for retina windows.
        p = [self convertPointToBacking:p];

        _glfwInput.MousePosX = p.x;
        _glfwInput.MousePosY = p.y;
    }

    if( _glfwWin.mousePosCallback )
    {
        _glfwWin.mousePosCallback( _glfwInput.MousePosX, _glfwInput.MousePosY );
    }
}

- (void)rightMouseDown:(NSEvent *)event
{
    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS );
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event
{
    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE );
}

- (void)otherMouseDown:(NSEvent *)event
{
    _glfwInputMouseClick( [event buttonNumber], GLFW_PRESS );
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    _glfwInputMouseClick( [event buttonNumber], GLFW_RELEASE );
}

- (void)keyDown:(NSEvent *)event
{
    NSUInteger length;
    NSString* characters;
    int i, code = convertMacKeyCode( [event keyCode] );

    if( code != -1 )
    {
        _glfwInputKey( code, GLFW_PRESS );

        if( [event modifierFlags] & NSCommandKeyMask )
        {
            if( !_glfwWin.sysKeysDisabled )
            {
                [super keyDown:event];
            }
        }
        else
        {
            characters = [event characters];
            length = [characters length];

            for( i = 0;  i < length;  i++ )
            {
                _glfwInputChar( [characters characterAtIndex:i], GLFW_PRESS );
            }
        }
    }
}

- (void)flagsChanged:(NSEvent *)event
{
    unsigned int newModifierFlags = [event modifierFlags] | NSDeviceIndependentModifierFlagsMask;
    int mode;

    if( newModifierFlags > _glfwWin.modifierFlags )
    {
        mode = GLFW_PRESS;
    }
    else
    {
        mode = GLFW_RELEASE;
    }

    _glfwWin.modifierFlags = newModifierFlags;
    _glfwInputKey( MAC_TO_GLFW_KEYCODE_MAPPING[[event keyCode]], mode );
}

- (void)keyUp:(NSEvent *)event
{
    NSUInteger length;
    NSString* characters;
    int i, code = convertMacKeyCode( [event keyCode] );

    if( code != -1 )
    {
        _glfwInputKey( code, GLFW_RELEASE );

        characters = [event characters];
        length = [characters length];

        for( i = 0;  i < length;  i++ )
        {
            _glfwInputChar( [characters characterAtIndex:i], GLFW_RELEASE );
        }
    }
}

- (void)scrollWheel:(NSEvent *)event
{
    _glfwInput.WheelPosFloating += [event deltaY];
    _glfwInput.WheelPos = lrint(_glfwInput.WheelPosFloating);

    if( _glfwWin.mouseWheelCallback )
    {
        _glfwWin.mouseWheelCallback( _glfwInput.WheelPos );
    }
}

@end

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================

CVReturn DisplayLinkCallback (
   CVDisplayLinkRef displayLink,
   const CVTimeStamp *inNow,
   const CVTimeStamp *inOutputTime,
   CVOptionFlags flagsIn,
   CVOptionFlags *flagsOut,
   void *displayLinkContext
   )

{
    [_glfwWin.delegate performSelectorOnMainThread:@selector(queueRender) withObject:nil waitUntilDone:NO ];
    return kCVReturnSuccess;
}

int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    _glfwWin.pixelFormat = nil;
    _glfwWin.window = nil;
    _glfwWin.context = nil;
    _glfwWin.aux_context = nil;
    _glfwWin.delegate = nil;

    _glfwWin.swapInterval = 1;
    _glfwWin.countDown = 1;
    _glfwWin.clientAPI = GLFW_NO_API;

    _glfwWin.delegate = [[GLFWWindowDelegate alloc] init];
    if( _glfwWin.delegate == nil )
    {
        return GL_FALSE;
    }

    [NSApp setDelegate:_glfwWin.delegate];

    CGDisplayModeRef fullscreenMode = NULL;
    if( wndconfig->mode == GLFW_FULLSCREEN )
    {
        // Find a good display mode for fullscreen based on width and height.
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
        CFIndex count = CFArrayGetCount(modes);
        int bestSizeDiff = INT_MAX;
        for (CFIndex i = 0; i < count; i++) {
            CGDisplayModeRef mode = (CGDisplayModeRef) CFArrayGetValueAtIndex(modes, i);
            int mode_width = CGDisplayModeGetWidth(mode);
            int mode_height = CGDisplayModeGetHeight(mode);
            int res_diff = abs((mode_width - width) * (mode_width - width) + (mode_height - height) * (mode_height - height));

            if (fullscreenMode == NULL ||
                (res_diff < bestSizeDiff && mode_width >= width && mode_height >= height)) {
                fullscreenMode = mode;
                bestSizeDiff = res_diff;
            }
        }
        CFRelease(modes);
        width = CGDisplayModeGetWidth(fullscreenMode);
        height = CGDisplayModeGetHeight(fullscreenMode);
    }

    unsigned int styleMask = 0;
    if( wndconfig->mode == GLFW_WINDOW )
    {
        styleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask;

        if( !wndconfig->windowNoResize )
        {
            styleMask |= NSResizableWindowMask;
        }
    }
    else
    {
        styleMask = NSBorderlessWindowMask;
    }

    _glfwWin.window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, width, height)
                  styleMask:styleMask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    GLFWContentView* view = [[GLFWContentView alloc] init];
    view.wantsLayer = YES;

    [_glfwWin.window setContentView: view];
    [_glfwWin.window setDelegate:_glfwWin.delegate];
    [_glfwWin.window setAcceptsMouseMovedEvents:YES];
    [_glfwWin.window center];

    if( wndconfig->mode == GLFW_FULLSCREEN )
    {
        CGCaptureAllDisplays();
        CGDisplaySetDisplayMode( CGMainDisplayID(), fullscreenMode, NULL );
    }

    [_glfwWin.window makeKeyAndOrderFront:nil];

    NSRect contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];
    _glfwWin.width = contentRect.size.width;
    _glfwWin.height = contentRect.size.height;

    return GL_TRUE;
}


//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    CVDisplayLinkRelease((CVDisplayLinkRef) _glfwWin.displayLink);

    [_glfwWin.window orderOut:nil];

    if( _glfwWin.fullscreen )
    {
        [[_glfwWin.window contentView] exitFullScreenModeWithOptions:nil];
        CGDisplaySetDisplayMode( CGMainDisplayID(),
                                 (CGDisplayModeRef)_glfwLibrary.DesktopMode, NULL );
        CGReleaseAllDisplays();
    }

    [_glfwWin.window setDelegate:nil];
    [NSApp setDelegate:nil];
    [_glfwWin.delegate release];
    _glfwWin.delegate = nil;

    [_glfwWin.window close];
    _glfwWin.window = nil;
}

int _glfwPlatformGetDefaultFramebuffer( )
{
    return 0;
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
    [_glfwWin.window setTitle:[NSString stringWithCString:title
                     encoding:NSISOLatin1StringEncoding]];
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
    [_glfwWin.window setContentSize:NSMakeSize(width, height)];
}

//========================================================================
// Set the window position
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
    NSRect contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];

    // We assume here that the client code wants to position the window within the
    // screen the window currently occupies
    NSRect screenRect = [[_glfwWin.window screen] visibleFrame];
    contentRect.origin = NSMakePoint(screenRect.origin.x + x,
                                     screenRect.origin.y + screenRect.size.height -
                                         y - contentRect.size.height);

    [_glfwWin.window setFrame:[_glfwWin.window frameRectForContentRect:contentRect]
                      display:YES];
}

//========================================================================
// Iconify the window
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
    [_glfwWin.window miniaturize:nil];
}

//========================================================================
// Restore (un-iconify) the window
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
    [_glfwWin.window deminiaturize:nil];
}

//========================================================================
// Swap buffers
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    _glfwWin.swapInterval = interval;
    _glfwWin.countDown = interval;
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
    NSRect contentRectFrame = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    NSRect contentRectBacking = [[_glfwWin.window contentView] convertRectToBacking:contentRectFrame];
    _glfwWin.highDPI = 0;
    if (contentRectBacking.size.width > contentRectFrame.size.width ||
        contentRectBacking.size.height > contentRectFrame.size.height) {
        _glfwWin.highDPI = 1;
    }

    NSRect contentRect =
        [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];
    _glfwWin.width = contentRect.size.width;
    _glfwWin.height = contentRect.size.height;
}

//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    NSEvent *event;

    do
    {
        event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                   untilDate:[NSDate distantPast]
                                      inMode:NSDefaultRunLoopMode
                                     dequeue:YES];

        if (event)
        {
            [NSApp sendEvent:event];
        }
    }
    while (event);

    [_glfwLibrary.AutoreleasePool drain];
    _glfwLibrary.AutoreleasePool = [[NSAutoreleasePool alloc] init];
}

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
    // I wanted to pass NO to dequeue:, and rely on PollEvents to
    // dequeue and send.  For reasons not at all clear to me, passing
    // NO to dequeue: causes this method never to return.
    NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                        untilDate:[NSDate distantFuture]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
    [NSApp sendEvent:event];

    _glfwPlatformPollEvents();
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
    [NSCursor hide];
    CGAssociateMouseAndMouseCursorPosition( false );
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
    [NSCursor unhide];
    CGAssociateMouseAndMouseCursorPosition( true );
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
    // The library seems to assume that after calling this the mouse won't move,
    // but obviously it will, and escape the app's window, and activate other apps,
    // and other badness in pain.  I think the API's just silly, but maybe I'm
    // misunderstanding it...

    // Also, (x, y) are window coords...

    // Also, it doesn't seem possible to write this robustly without
    // calculating the maximum y coordinate of all screens, since Cocoa's
    // "global coordinates" are upside down from CG's...

    // Without this (once per app run, but it's convenient to do it here)
    // events will be suppressed for a default of 0.25 seconds after we
    // move the cursor.
    CGSetLocalEventsSuppressionInterval( 0.0 );

    NSPoint localPoint = NSMakePoint( x, y );
    NSPoint globalPoint = [_glfwWin.window convertBaseToScreen:localPoint];
    CGPoint mainScreenOrigin = CGDisplayBounds( CGMainDisplayID() ).origin;
    double mainScreenHeight = CGDisplayBounds( CGMainDisplayID() ).size.height;
    CGPoint targetPoint = CGPointMake( globalPoint.x - mainScreenOrigin.x,
                                       mainScreenHeight - globalPoint.y - mainScreenOrigin.y );
    CGDisplayMoveCursorToPoint( CGMainDisplayID(), targetPoint );
}

void _glfwShowKeyboard( int show, int type, int auto_close )
{
}

void _glfwResetKeyboard( void )
{
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    return 0;
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
GLFWAPI id glfwGetOSXNSWindow()
{
    return _glfwWin.window;
}

GLFWAPI id glfwGetOSXNSView()
{
    return [_glfwWin.window contentView];
}

GLFWAPI id glfwGetOSXNSOpenGLContext()
{
    return 0;
}

GLFWAPI id glfwGetOSXCALayer()
{
    return [[_glfwWin.window contentView] layer];
}

//========================================================================
// Query auxillary context
//========================================================================
int _glfwPlatformQueryAuxContext()
{
    return 0;
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContext()
{
    return 0;
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void _glfwPlatformUnacquireAuxContext(void* context)
{
}

GLFWAPI void glfwAccelerometerEnable()
{
}
