// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "window_macos.h"
#include "../platform_window_constants.h"
#import <Carbon/Carbon.h>
#include <math.h>

static int KeyCode(unsigned short native)
{
    using namespace dmPlatform;
    // Physical positions from the macOS virtual key table, independent of the active input source.
    static const int codes[] = {
        /* 0x00 */ 'A', 'S', 'D', 'F',
        /* 0x04 */ 'H', 'G', 'Z', 'X',
        /* 0x08 */ 'C', 'V', 161, 'B',
        /* 0x0C */ 'Q', 'W', 'E', 'R',
        /* 0x10 */ 'Y', 'T', '1', '2',
        /* 0x14 */ '3', '4', '6', '5',
        /* 0x18 */ '=', '9', '7', '-',
        /* 0x1C */ '8', '0', ']', 'O',
        /* 0x20 */ 'U', '[', 'I', 'P',
        /* 0x24 */ PLATFORM_KEY_ENTER, 'L', 'J', '\'',
        /* 0x28 */ 'K', ';', '\\', ',',
        /* 0x2C */ '/', 'N', 'M', '.',
        /* 0x30 */ PLATFORM_KEY_TAB, ' ', '`', PLATFORM_KEY_BACKSPACE,
        /* 0x34 */ -1, PLATFORM_KEY_ESC, PLATFORM_KEY_RSUPER, PLATFORM_KEY_LSUPER,
        /* 0x38 */ PLATFORM_KEY_LSHIFT, PLATFORM_KEY_CAPS_LOCK, PLATFORM_KEY_LALT, PLATFORM_KEY_LCTRL,
        /* 0x3C */ PLATFORM_KEY_RSHIFT, PLATFORM_KEY_RALT, PLATFORM_KEY_RCTRL, -1,
        /* 0x40 */ 306, PLATFORM_KEY_KP_DECIMAL, -1, PLATFORM_KEY_KP_MULTIPLY,
        /* 0x44 */ -1, PLATFORM_KEY_KP_ADD, -1, PLATFORM_KEY_KP_NUM_LOCK,
        /* 0x48 */ -1, -1, -1, PLATFORM_KEY_KP_DIVIDE,
        /* 0x4C */ PLATFORM_KEY_KP_ENTER, -1, PLATFORM_KEY_KP_SUBTRACT, 307,
        /* 0x50 */ 308, PLATFORM_KEY_KP_EQUAL, PLATFORM_KEY_KP_0, PLATFORM_KEY_KP_1,
        /* 0x54 */ PLATFORM_KEY_KP_2, PLATFORM_KEY_KP_3, PLATFORM_KEY_KP_4, PLATFORM_KEY_KP_5,
        /* 0x58 */ PLATFORM_KEY_KP_6, PLATFORM_KEY_KP_7, 309, PLATFORM_KEY_KP_8,
        /* 0x5C */ PLATFORM_KEY_KP_9, -1, -1, -1,
        /* 0x60 */ PLATFORM_KEY_F5, PLATFORM_KEY_F6, PLATFORM_KEY_F7, PLATFORM_KEY_F3,
        /* 0x64 */ PLATFORM_KEY_F8, PLATFORM_KEY_F9, -1, PLATFORM_KEY_F11,
        /* 0x68 */ -1, 283, 305, 303,
        /* 0x6C */ -1, PLATFORM_KEY_F10, PLATFORM_KEY_MENU, PLATFORM_KEY_F12,
        /* 0x70 */ -1, 304, PLATFORM_KEY_INSERT, PLATFORM_KEY_HOME,
        /* 0x74 */ PLATFORM_KEY_PAGEUP, PLATFORM_KEY_DEL, PLATFORM_KEY_F4, PLATFORM_KEY_END,
        /* 0x78 */ PLATFORM_KEY_F2, PLATFORM_KEY_PAGEDOWN, PLATFORM_KEY_F1, PLATFORM_KEY_LEFT,
        /* 0x7C */ PLATFORM_KEY_RIGHT, PLATFORM_KEY_DOWN, PLATFORM_KEY_UP, -1,
    };
    return native < sizeof(codes) / sizeof(codes[0]) ? codes[native] : -1;
}

static NSEventModifierFlags ModifierMask(unsigned short key)
{
    switch (key)
    {
        case kVK_Shift:
        case kVK_RightShift:
        {
            return NSEventModifierFlagShift;
        }
        case kVK_Control:
        case kVK_RightControl:
        {
            return NSEventModifierFlagControl;
        }
        case kVK_Command:
        case kVK_RightCommand:
        {
            return NSEventModifierFlagCommand;
        }
        case kVK_Option:
        case kVK_RightOption:
        {
            return NSEventModifierFlagOption;
        }
        case kVK_CapsLock:
        {
            return NSEventModifierFlagCapsLock;
        }
        default:
        {
            return 0;
        }
    }
}

static NSString* PlainText(id text)
{
    return [text isKindOfClass:[NSAttributedString class]] ? [text string] : text;
}

@interface DMInputView : NSView <NSTextInputClient>
{
    @public
    HWindow handle;
    NSString* marked;
    NSRange selection;
}
@end

@implementation DMInputView
- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)dealloc {
    [marked release];
    [super dealloc];
}

- (void)keyDown:(NSEvent*)event
{
    int code = KeyCode(event.keyCode);
    if (handle && code >= 0)
    {
        handle->m_Keys[code] = 1;
    }

    if (handle)
    {
        [self interpretKeyEvents:@[event]];
    }
}

- (void)keyUp:(NSEvent*)event
{
    int code = KeyCode(event.keyCode);
    if (handle && code >= 0)
    {
        handle->m_Keys[code] = 0;
    }
}

- (void)flagsChanged:(NSEvent*)event
{
    int code = KeyCode(event.keyCode);
    if (handle && code >= 0)
    {
        bool active = (event.modifierFlags & ModifierMask(event.keyCode)) != 0;
        // When both sides are held, AppKit keeps the aggregate flag set on the first release.
        handle->m_Keys[code] = active && (event.keyCode == kVK_CapsLock || !handle->m_Keys[code]);
    }
}

- (BOOL)hasMarkedText {
    return marked.length != 0;
}

- (NSRange)markedRange {
    return marked.length ? NSMakeRange(0, marked.length) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return marked.length ? selection : NSMakeRange(NSNotFound, 0);
}

- (NSArray*)validAttributesForMarkedText {
    return @[];
}

- (void)setMarkedText:(id)text selectedRange:(NSRange)range replacementRange:(NSRange)replacement
{
    NSString* next = [PlainText(text) copy];
    [marked release];
    marked = next;
    selection = range;
    if (handle && handle->m_InputCallbacks.m_MarkedCallback)
    {
        handle->m_InputCallbacks.m_MarkedCallback(handle->m_InputCallbacks.m_MarkedUserData, (char*)marked.UTF8String);
    }
}

- (void)unmarkText
{
    [marked release];
    marked = nil;
    if (handle && handle->m_InputCallbacks.m_MarkedCallback)
    {
        handle->m_InputCallbacks.m_MarkedCallback(handle->m_InputCallbacks.m_MarkedUserData, (char*)"");
    }
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual
{
    NSRange intersection = NSIntersectionRange(range, NSMakeRange(0, marked.length));
    if (!intersection.length)
    {
        return nil;
    }

    if (actual)
    {
        *actual = intersection;
    }

    return [[[NSAttributedString alloc] initWithString:[marked substringWithRange:intersection]] autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return NSNotFound;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual
{
    if (actual)
    {
        *actual = NSMakeRange(0, 0);
    }

    return [self.window convertRectToScreen:[self convertRect:NSMakeRect(0, 0, 1, 1) toView:nil]];
}

- (void)doCommandBySelector:(SEL)command
{
    if (command == @selector(cancelOperation:) && self.hasMarkedText)
    {
        [self unmarkText];
    }
}

- (void)insertText:(id)text replacementRange:(NSRange)replacement
{
    if (self.hasMarkedText)
    {
        [self unmarkText];
    }

    if (!handle || !handle->m_InputCallbacks.m_CharCallback || (NSApp.currentEvent.modifierFlags & NSEventModifierFlagCommand))
    {
        return;
    }

    NSString* string = PlainText(text);
    for (NSUInteger i = 0; i < string.length; ++i)
    {
        UTF32Char scalar = [string characterAtIndex:i];
        if (CFStringIsSurrogateHighCharacter(scalar) && i + 1 < string.length)
        {
            unichar low = [string characterAtIndex:i + 1];
            if (CFStringIsSurrogateLowCharacter(low))
            {
                scalar = CFStringGetLongCharacterForSurrogatePair(scalar, low);
                ++i;
            }
        }

        if (scalar >= 32 && !(scalar >= 127 && scalar < 160) && !(scalar >= 0xD800 && scalar <= 0xDFFF) &&
            !(scalar >= 0xF700 && scalar <= 0xF7FF))
        {
            handle->m_InputCallbacks.m_CharCallback(handle->m_InputCallbacks.m_CharUserData, (int)scalar);
        }
    }
}

- (void)setButton:(NSEvent*)event down:(BOOL)down
{
    NSInteger button = event.buttonNumber;
    if (handle && button >= 0 && button < sizeof(handle->m_Buttons))
    {
        handle->m_Buttons[button] = down;
    }
}

- (void)mouseDown:(NSEvent*)e {
    [self setButton:e down:YES];
}

- (void)mouseUp:(NSEvent*)e {
    [self setButton:e down:NO];
}

- (void)rightMouseDown:(NSEvent*)e {
    [self setButton:e down:YES];
}

- (void)rightMouseUp:(NSEvent*)e {
    [self setButton:e down:NO];
}

- (void)otherMouseDown:(NSEvent*)e {
    [self setButton:e down:YES];
}

- (void)otherMouseUp:(NSEvent*)e {
    [self setButton:e down:NO];
}

- (void)mouseMoved:(NSEvent*)e
{
    if (handle && handle->m_CursorDisabled)
    {
        // The first relative event includes the distance AppKit warped the pointer.
        handle->m_VirtualCursor.x += e.deltaX - handle->m_WarpDelta.x;
        handle->m_VirtualCursor.y += e.deltaY - handle->m_WarpDelta.y;
        handle->m_WarpDelta = NSZeroPoint;
    }
}

- (void)mouseDragged:(NSEvent*)e {
    [self mouseMoved:e];
}

- (void)rightMouseDragged:(NSEvent*)e {
    [self mouseMoved:e];
}

- (void)otherMouseDragged:(NSEvent*)e {
    [self mouseMoved:e];
}

- (void)scrollWheel:(NSEvent*)e
{
    if (handle)
    {
        handle->m_Scroll += e.scrollingDeltaY * (e.hasPreciseScrollingDeltas ? 0.1 : 1.0);
    }
}

@end

namespace dmPlatform
{
    NSView* NewContentView(HWindow window, NSRect frame)
    {
        DMInputView* view = [[DMInputView alloc] initWithFrame:frame];
        if (view)
        {
            view->handle = window;
        }

        return view;
    }

    void DetachContentView(NSView* view)
    {
        if (view)
        {
            ((DMInputView*)view)->handle = 0;
        }
    }

    static NSPoint CursorPosition(HWindow window)
    {
        NSPoint point = [window->m_View convertPoint:window->m_Window.mouseLocationOutsideOfEventStream fromView:nil];
        point.y = NSMaxY(window->m_View.bounds) - point.y;
        return point;
    }

    static CGError WarpCursor(NSPoint cocoa)
    {
        return CGWarpMouseCursorPosition(CGPointMake(cocoa.x, NSMaxY([NSScreen screens].firstObject.frame) - cocoa.y));
    }

    void CenterLockedCursor(HWindow window)
    {
        if (!window->m_CursorDisabled)
        {
            return;
        }

        NSRect bounds = window->m_View.bounds;
        NSPoint center = [window->m_View convertPoint:NSMakePoint(floor(NSMidX(bounds)), floor(NSMidY(bounds))) toView:nil];
        center = [window->m_Window convertPointToScreen:center];
        NSPoint previous = [NSEvent mouseLocation];
        if (!NSEqualPoints(center, previous) && WarpCursor(center) == kCGErrorSuccess)
        {
            // AppKit can combine several warps into the next relative mouse event.
            window->m_WarpDelta.x += center.x - previous.x;
            window->m_WarpDelta.y += previous.y - center.y;
        }
    }

    void UpdateCursorMode(HWindow window, bool focused)
    {
        bool disable = focused && window->m_CursorLocked;
        if (disable == window->m_CursorDisabled)
        {
            return;
        }

        window->m_CursorDisabled = disable;
        if (disable)
        {
            window->m_VirtualCursor = CursorPosition(window);
            window->m_SavedCursor = [NSEvent mouseLocation];
            [NSCursor hide];
            CenterLockedCursor(window);
            CGAssociateMouseAndMouseCursorPosition(false);
        }
        else
        {
            WarpCursor(window->m_SavedCursor);
            CGAssociateMouseAndMouseCursorPosition(true);
            window->m_WarpDelta = NSZeroPoint;
            [NSCursor unhide];
        }
    }

    void GetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        NSPoint point = window->m_CursorDisabled ? window->m_VirtualCursor : CursorPosition(window);
        *x = (int32_t)(point.x * window->m_Scale);
        *y = (int32_t)(point.y * window->m_Scale);
    }

    int32_t GetKey(HWindow window, int32_t code)
    {
        return (unsigned)code < sizeof(window->m_Keys) && window->m_Keys[code];
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return (unsigned)button < sizeof(window->m_Buttons) && window->m_Buttons[button];
    }

    int32_t GetMouseWheel(HWindow window)
    {
        return (int32_t)round(window->m_Scroll);
    }

    void SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* data)
    {
        window->m_InputCallbacks.m_CharCallback = cb;
        window->m_InputCallbacks.m_CharUserData = data;
    }

    void SetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* data)
    {
        window->m_InputCallbacks.m_MarkedCallback = cb;
        window->m_InputCallbacks.m_MarkedUserData = data;
    }

    void SetKeyboardDeviceChangedCallback(HWindow, FWindowDeviceChangedCallback, void*)
    {
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool value, bool)
    {
        if (state == WINDOW_DEVICE_STATE_CURSOR)
        {
            window->m_CursorLocked = !value;
            UpdateCursorMode(window, window->m_Window.keyWindow);
        }
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool value)
    {
        SetDeviceState(window, state, value, false);
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state, int32_t index)
    {
        if (state == WINDOW_DEVICE_STATE_JOYSTICK_PRESENT)
        {
            return JoystickPresent(index);
        }

        return state == WINDOW_DEVICE_STATE_CURSOR_LOCK && window->m_CursorLocked;
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

}
