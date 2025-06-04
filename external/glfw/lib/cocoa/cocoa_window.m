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

    NSRect contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    NSRect contentBackingRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];

    if (contentBackingRect.size.width == 0 && contentBackingRect.size.height == 0)
    {
        contentBackingRect = contentRect;
    }

    _glfwWin.width = contentBackingRect.size.width;
    _glfwWin.height = contentBackingRect.size.height;

    if( _glfwWin.windowSizeCallback )
    {
        _glfwWin.windowSizeCallback( _glfwWin.width, _glfwWin.height );
    }
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    _glfwWin.iconified = GL_TRUE;
    if(_glfwWin.windowIconifyCallback)
        _glfwWin.windowIconifyCallback(1);
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    _glfwWin.iconified = GL_FALSE;
    if(_glfwWin.windowIconifyCallback)
        _glfwWin.windowIconifyCallback(0);
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

- (void)applicationDidUpdate:(NSNotification *)notification
{
    // Wait for the first update to make window inactive and then activate it again
    // It helps to avoid issue when the window opens inactive
    // https://github.com/defold/defold/issues/6709
    if( _glfwLibrary.FirstUpdate )
    {
        // Starting from macOS Sonoma (14.0) it should be solved in two different ways depends 
        // if it's bundled app or not
        // https://github.com/defold/defold/issues/8066
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        if (version.majorVersion < 14)
        {
            [self deactivateProcess];
        }
        else
        {
            if ( _glfwLibrary.Unbundled )
            {
                [self deactivateProcess];
            }
            else
            {
                [self activateWindow];
            }
        }

        _glfwLibrary.FirstUpdate = GL_FALSE;
    }
}

- (void)deactivateProcess
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType( &psn, kProcessTransformToBackgroundApplication );
    [self performSelector:@selector(activateProcess) withObject:nil afterDelay:0.1];
}

- (void)activateProcess
{
    ProcessSerialNumber psn = { 0, kCurrentProcess }; 
    (void) TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    [self performSelector:@selector(activateWindow) withObject:nil afterDelay:0.1];
}

- (void)activateWindow
{
    if(_glfwWin.clientAPI != GLFW_OPENGL_API && _glfwWin.fullscreen )
    {
        [_glfwWin.window toggleFullScreen:nil];
    }

    [_glfwWin.window makeKeyAndOrderFront:nil];
    // Starting from macOS Sonoma (14.0) an extra step needed
    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    if (version.majorVersion >= 14) {
        [NSApp activateIgnoringOtherApps:YES];
    }
    [[NSRunningApplication currentApplication] activateWithOptions:(NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps)];
}

// macos 12+
// https://sector7.computest.nl/post/2022-08-process-injection-breaking-all-macos-security-layers-with-a-single-vulnerability/
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app
{
    return YES;
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

// Defines a constant for empty ranges in NSTextInputClient
//
static const NSRange kEmptyRange = { NSNotFound, 0 };

@interface GLFWContentView : NSView <NSTextInputClient>
{
    NSMutableAttributedString* markedText;
}

@end

@implementation GLFWContentView

- (instancetype)init
{
    if (self = [super init])
    {
        markedText = [[NSMutableAttributedString alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [markedText release];
    [super dealloc];
}

- (BOOL)wantsUpdateLayer
{
    return _glfwWin.clientAPI == GLFW_NO_API;
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

+ (Class) layerClass
{
    return [CAMetalLayer class];
}

-(CALayer*) makeBackingLayer
{
    return [self.class.layerClass layer];
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

        if (_glfwWin.clientAPI == GLFW_OPENGL_API)
        {
            // Need convert mouse coord into backing coords
            // which will be different for retina windows.
            p = [self convertPointToBacking:p];
            _glfwInput.MousePosX = p.x;
            _glfwInput.MousePosY = p.y;
        }
        else
        {
            float contentScale = [[[_glfwWin.window contentView] layer] contentsScale];
            p = [self convertPointFromLayer:p];
            _glfwInput.MousePosX = p.x * contentScale;
            _glfwInput.MousePosY = p.y * contentScale;
        }
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
    }

     [self interpretKeyEvents:@[event]];
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
    int i, code = convertMacKeyCode( [event keyCode] );

    if( code != -1 )
    {
        _glfwInputKey( code, GLFW_RELEASE );
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


- (BOOL)hasMarkedText
{
    return [markedText length] > 0;
}

- (NSRange)markedRange
{
    if ([markedText length] > 0)
        return NSMakeRange(0, [markedText length] - 1);
    else
        return kEmptyRange;
}

- (NSRange)selectedRange
{
    return kEmptyRange;
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange
{
    [markedText release];
    if ([string isKindOfClass:[NSAttributedString class]])
        markedText = [[NSMutableAttributedString alloc] initWithAttributedString:string];
    else
        markedText = [[NSMutableAttributedString alloc] initWithString:string];

    _glfwSetMarkedText((char*)[[markedText string] UTF8String]);
}

- (void)unmarkText
{
    [[markedText mutableString] setString:@""];

    _glfwSetMarkedText("");
}

- (NSArray*)validAttributesForMarkedText
{
    return [NSArray array];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange
{
    //const NSRect frame = [window->ns.view frame];
    const NSRect frame = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    NSString* characters;
    NSEvent* event = [NSApp currentEvent];

    if ([string isKindOfClass:[NSAttributedString class]])
        characters = [string string];
    else
        characters = (NSString*) string;

    NSRange range = NSMakeRange(0, [characters length]);
    while (range.length)
    {
        uint32_t codepoint = 0;

        if ([characters getBytes:&codepoint
                       maxLength:sizeof(codepoint)
                      usedLength:NULL
                        encoding:NSUTF32StringEncoding
                         options:0
                           range:range
                  remainingRange:&range])
        {
            if (codepoint >= 0xf700 && codepoint <= 0xf7ff)
                continue;

            _glfwInputChar(codepoint, GLFW_PRESS);
        }
    }
}

- (void)doCommandBySelector:(SEL)selector
{
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
    _glfwLibrary.FirstUpdate = GL_TRUE;

    _glfwWin.swapInterval = 1;
    _glfwWin.countDown = 1;
    _glfwWin.clientAPI = wndconfig->clientAPI;

    _glfwWin.delegate = [[GLFWWindowDelegate alloc] init];
    if( _glfwWin.delegate == nil )
    {
        return GL_FALSE;
    }

    [NSApp setDelegate:_glfwWin.delegate];

    int colorBits = 0;
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        // Mac OS X needs non-zero color size, so set resonable values
        colorBits = fbconfig->redBits + fbconfig->greenBits + fbconfig->blueBits;
        if( colorBits == 0 )
        {
            colorBits = 24;
        }
        else if( colorBits < 15 )
        {
            colorBits = 15;
        }

        // Ignored hints:
        // OpenGLMajor, OpenGLMinor, OpenGLForward:
        //     pending Mac OS X support for OpenGL 3.x
        // OpenGLDebug
        //     pending it meaning anything on Mac OS X

        // Don't use accumulation buffer support; it's not accelerated
        // Aux buffers probably aren't accelerated either
    }

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

        if (_glfwWin.clientAPI != GLFW_OPENGL_API)
        {
            styleMask |= NSWindowStyleMaskTitled |
                         NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskResizable;
        }
    }

    _glfwWin.window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, width, height)
                  styleMask:styleMask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    GLFWContentView* view = [[GLFWContentView alloc] init];
    view.wantsLayer = _glfwWin.clientAPI == GLFW_NO_API ? YES : NO;

    [_glfwWin.window setContentView: view];
    [_glfwWin.window makeFirstResponder: view];
    [_glfwWin.window setDelegate:_glfwWin.delegate];
    [_glfwWin.window setAcceptsMouseMovedEvents:YES];
    [_glfwWin.window center];

    if (_glfwWin.clientAPI == GLFW_OPENGL_API && wndconfig->highDPI)
    {
        [ [_glfwWin.window contentView] setWantsBestResolutionOpenGLSurface:YES];
    }

    if( wndconfig->mode == GLFW_FULLSCREEN )
    {
        CGCaptureAllDisplays();
        CGDisplaySetDisplayMode( CGMainDisplayID(), fullscreenMode, NULL );
    }

    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        unsigned int attribute_count = 0;
    #define MAX_ATTRS 24 // urgh
        NSOpenGLPixelFormatAttribute attributes[MAX_ATTRS];
    #define ADD_ATTR(x) assert(attribute_count < MAX_ATTRS); attributes[attribute_count++] = x
    #define ADD_ATTR2(x, y) (void)({ ADD_ATTR(x); ADD_ATTR(y); })

        ADD_ATTR( NSOpenGLPFADoubleBuffer );

        if( wndconfig->mode == GLFW_FULLSCREEN )
        {
            ADD_ATTR2( NSOpenGLPFAScreenMask, CGDisplayIDToOpenGLDisplayMask( CGMainDisplayID() ) );
        }

        ADD_ATTR2( NSOpenGLPFAColorSize, colorBits );

        if( fbconfig->alphaBits > 0)
        {
            ADD_ATTR2( NSOpenGLPFAAlphaSize, fbconfig->alphaBits );
        }

        if( fbconfig->depthBits > 0)
        {
            ADD_ATTR2( NSOpenGLPFADepthSize, fbconfig->depthBits );
        }

        if( fbconfig->stencilBits > 0)
        {
            ADD_ATTR2( NSOpenGLPFAStencilSize, fbconfig->stencilBits );
        }

        int accumBits = fbconfig->accumRedBits + fbconfig->accumGreenBits +
                        fbconfig->accumBlueBits + fbconfig->accumAlphaBits;

        if( accumBits > 0)
        {
            ADD_ATTR2( NSOpenGLPFAAccumSize, accumBits );
        }

        if( fbconfig->auxBuffers > 0)
        {
            ADD_ATTR2( NSOpenGLPFAAuxBuffers, fbconfig->auxBuffers );
        }

        if( fbconfig->stereo)
        {
            ADD_ATTR( NSOpenGLPFAStereo );
        }

        if( fbconfig->samples > 0)
        {
            ADD_ATTR2( NSOpenGLPFASampleBuffers, 1 );
            ADD_ATTR2( NSOpenGLPFASamples, fbconfig->samples );
        }

        if( wndconfig->glMajor >= 4 )
        {
            ADD_ATTR2(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core);
        }
        else if( wndconfig->glMajor >= 3 )
        {
            ADD_ATTR2(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
        }

        ADD_ATTR(0);

        _glfwWin.pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if( _glfwWin.pixelFormat == nil )
        {
            return GL_FALSE;
        }

        _glfwWin.context = [[NSOpenGLContext alloc] initWithFormat:_glfwWin.pixelFormat
                                                      shareContext:nil];
        if( _glfwWin.context == nil )
        {
            return GL_FALSE;
        }
        _glfwWin.aux_context = [[NSOpenGLContext alloc] initWithFormat:_glfwWin.pixelFormat shareContext:_glfwWin.context];

        [_glfwWin.context setView:[_glfwWin.window contentView]];

        // Fetch the resulting width and height for backing buffer
        // will differ from the input params on retina enabled windows.
        NSRect contentRectFrame = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
        NSRect contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRectFrame];
        if (contentRect.size.width == 0 && contentRect.size.height == 0)
        {
            contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
        }

        _glfwWin.width = contentRect.size.width;
        _glfwWin.height = contentRect.size.height;

        if( wndconfig->mode == GLFW_FULLSCREEN )
        {
            // TODO: Make this work on pre-Leopard systems
            [[_glfwWin.window contentView] enterFullScreenMode:[NSScreen mainScreen]
                                                   withOptions:nil];
        }

        contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];
        if (contentRect.size.width == 0 && contentRect.size.height == 0)
        {
            contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
        }

        [_glfwWin.context makeCurrentContext];

        NSPoint point = [_glfwWin.window mouseLocationOutsideOfEventStream];
        _glfwInput.MousePosX = point.x;
        _glfwInput.MousePosY = [[_glfwWin.window contentView] bounds].size.height - point.y;

        CVDisplayLinkRef displayLink;
        CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
        CVDisplayLinkSetOutputCallback(displayLink, &DisplayLinkCallback, 0);
        CGLContextObj cglContext = [_glfwWin.context CGLContextObj];
        CGLPixelFormatObj cglPixelFormat = [_glfwWin.pixelFormat CGLPixelFormatObj];
        CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
        CVDisplayLinkStart(displayLink);
        _glfwWin.displayLink = (uintptr_t) displayLink;
    }
    else
    {
        NSRect contentRect = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
        contentRect = [[_glfwWin.window contentView] convertRectToBacking:contentRect];
        _glfwWin.width = contentRect.size.width;
        _glfwWin.height = contentRect.size.height;

        CVDisplayLinkRef displayLink;
        CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
        CVDisplayLinkSetOutputCallback(displayLink, &DisplayLinkCallback, 0);
        CGDirectDisplayID viewDisplayID = (CGDirectDisplayID) [[_glfwWin.window screen].deviceDescription[@"NSScreenNumber"] unsignedIntegerValue];
        CVDisplayLinkSetCurrentCGDisplay(displayLink, viewDisplayID);

        CVDisplayLinkStart(displayLink);
        _glfwWin.displayLink = (uintptr_t) displayLink;
    }

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

    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        [_glfwWin.pixelFormat release];
        _glfwWin.pixelFormat = nil;

        if( _glfwWin.aux_context != nil )
        {
            [_glfwWin.aux_context release];
            _glfwWin.aux_context = nil;
        }

        [NSOpenGLContext clearCurrentContext];
        [_glfwWin.context release];
        _glfwWin.context = nil;
    }

    [_glfwWin.window setDelegate:nil];
    [NSApp setDelegate:nil];
    [_glfwWin.delegate release];
    _glfwWin.delegate = nil;

    [_glfwWin.window close];
    _glfwWin.window = nil;

    // TODO: Probably more cleanup
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
                     encoding:NSUTF8StringEncoding]];
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
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        while (_glfwWin.countDown > 0)
        {
            // Skip frames.
            // NOTE: We wait up to 1 second for event but in practice
            // event will fire as soon as a frame is "ready"
            // (performSelectorOnMainThread)

            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, TRUE);
        }
        _glfwWin.countDown = _glfwWin.swapInterval;

        // ARP appears to be unnecessary, but this is future-proof
        [_glfwWin.context flushBuffer];
    }
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    _glfwWin.swapInterval = interval;
    _glfwWin.countDown = interval;
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        GLint sync = interval > 1 ? 1 : interval;
        [_glfwWin.context setValues:&sync forParameter:NSOpenGLCPSwapInterval];
    }
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================
void _glfwPlatformRefreshWindowParamsOpenGL( void )
{
    GLint value;

    // Since GLFW 2.x doesn't understand screens, we use virtual screen zero
    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAAccelerated
                   forVirtualScreen:0];
    _glfwWin.accelerated = value;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAAlphaSize
                   forVirtualScreen:0];
    _glfwWin.alphaBits = value;

    // It seems that the color size includes the size of the alpha channel
    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAColorSize
                   forVirtualScreen:0];
    value -= _glfwWin.alphaBits;
    _glfwWin.redBits = value / 3;
    _glfwWin.greenBits = value / 3;
    _glfwWin.blueBits = value / 3;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFADepthSize
                   forVirtualScreen:0];
    _glfwWin.depthBits = value;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAStencilSize
                   forVirtualScreen:0];
    _glfwWin.stencilBits = value;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAAccumSize
                   forVirtualScreen:0];
    _glfwWin.accumRedBits = value / 3;
    _glfwWin.accumGreenBits = value / 3;
    _glfwWin.accumBlueBits = value / 3;

    // TODO: Figure out what to set this value to
    _glfwWin.accumAlphaBits = 0;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAAuxBuffers
                   forVirtualScreen:0];
    _glfwWin.auxBuffers = value;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFAStereo
                   forVirtualScreen:0];
    _glfwWin.stereo = value;

    [_glfwWin.pixelFormat getValues:&value
                       forAttribute:NSOpenGLPFASamples
                   forVirtualScreen:0];
    _glfwWin.samples = value;

    // These are forced to false as long as Mac OS X lacks support for OpenGL 3.0+
    _glfwWin.glForward = GL_FALSE;
    _glfwWin.glDebug = GL_FALSE;
    _glfwWin.glProfile = 0;
}

void _glfwPlatformRefreshWindowParams( void )
{
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        _glfwPlatformRefreshWindowParamsOpenGL();
    }

    NSRect contentRectFrame = [_glfwWin.window contentRectForFrameRect:[_glfwWin.window frame]];
    NSRect contentRectBacking = [[_glfwWin.window contentView] convertRectToBacking:contentRectFrame];

    if (contentRectBacking.size.width == 0 && contentRectBacking.size.height == 0)
    {
        contentRectBacking = contentRectFrame;
    }

    _glfwWin.highDPI = 0;
    if (contentRectBacking.size.width > contentRectFrame.size.width ||
        contentRectBacking.size.height > contentRectFrame.size.height) {
        _glfwWin.highDPI = 1;
    }

    _glfwWin.width = contentRectBacking.size.width;
    _glfwWin.height = contentRectBacking.size.height;
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
    return _glfwWin.context;
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
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        if(_glfwWin.aux_context == nil)
            return 0;
        return 1;
    }
    return 0;
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContext()
{
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        if(_glfwWin.aux_context == nil)
        {
            fprintf( stderr, "Unable to make OpenGL aux context current, is NULL\n" );
            return 0;
        }
        [_glfwWin.aux_context makeCurrentContext];
        return _glfwWin.aux_context;
    }
    return 0;
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void _glfwPlatformUnacquireAuxContext(void* context)
{
    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        [NSOpenGLContext clearCurrentContext];
    }
}

void _glfwPlatformSetViewType(int view_type)
{
}

GLFWAPI void glfwAccelerometerEnable()
{
}

void _glfwPlatformSetWindowBackgroundColor(unsigned int color)
{
    float r = (color & 0xff) / 255.0f;
    float g = ((color >> 8) & 0xff) / 255.0f;
    float b = ((color >> 16) & 0xff) / 255.0f;
    [_glfwWin.window setBackgroundColor: [NSColor colorWithDeviceRed:r green:g blue:b alpha:1.0f]];
}

float _glfwPlatformGetDisplayScaleFactor()
{
    CGDirectDisplayID currentDisplayID = [[[[_glfwWin.window screen] deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(currentDisplayID);
    float scaling_factor = (float)CGDisplayModeGetPixelWidth(mode) / (float)CGDisplayModeGetWidth(mode);
    CGDisplayModeRelease(mode);
    return scaling_factor;
}
