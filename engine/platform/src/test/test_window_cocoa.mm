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

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <OpenGL/gl3.h>
#include <pthread.h>
#include <jc_test/jc_test.h>
#include "test_window_focus.h"
#include <dlib/time.h>
#include "../macos/window_macos.h"
#include "../platform_window_osx.h"
#include "../platform_window_constants.h"

@interface DMTestPointerEvent : NSEvent
@property NSPoint movement;
@end
@implementation DMTestPointerEvent
- (BOOL)hasPreciseScrollingDeltas {
    return YES;
}

- (CGFloat)scrollingDeltaY {
    return 6;
}

- (CGFloat)deltaX {
    return self.movement.x;
}

- (CGFloat)deltaY {
    return self.movement.y;
}

@end

@interface DMTestOpenGLContext : NSOpenGLContext
@property(nonatomic) BOOL updated;
@end

@implementation DMTestOpenGLContext
- (void)update {
    [super update];
    self.updated = YES;
}

@end

struct WindowEvents
{
    int      m_Closes, m_Focus, m_FocusCount, m_Minimized;
    uint32_t m_DrawableWidth, m_DrawableHeight;
    bool     m_AllowClose;
    int      m_Characters[4];
    int      m_Count, m_MarkedUpdates;
    char     m_Marked[64];
};
static int CloseRequest(void* data)
{
    WindowEvents* events = (WindowEvents*)data;
    ++events->m_Closes;
    return events->m_AllowClose;
}

static void Focus(void* data, uint32_t value)
{
    WindowEvents* events = (WindowEvents*)data;
    events->m_Focus = value;
    ++events->m_FocusCount;
}

static void Minimize(void* data, uint32_t value)
{
    ((WindowEvents*)data)->m_Minimized = value;
}

static void Resize(void* data, uint32_t width, uint32_t height)
{
    WindowEvents* events = (WindowEvents*)data;
    events->m_DrawableWidth = width;
    events->m_DrawableHeight = height;
}

static void Character(void* data, int value)
{
    WindowEvents* events = (WindowEvents*)data;
    if (events->m_Count < 4)
    {
        events->m_Characters[events->m_Count++] = value;
    }
}

static void Marked(void* data, char* text)
{
    WindowEvents* events = (WindowEvents*)data;
    strlcpy(events->m_Marked, text, sizeof(events->m_Marked));
    ++events->m_MarkedUpdates;
}

class CocoaWindowTest : public jc_test_base_class
{
    protected:
    HWindow            window;
    WindowCreateParams params;
    WindowEvents       events;
    void SetUp()
    {
        window = dmPlatform::NewWindow();
        memset(&events, 0, sizeof(events));
        WindowCreateParamsInitialize(&params);
        params.m_Title = "Defold Cocoa test";
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        params.m_Hidden = true;
        params.m_FocusOnShow = true;
        params.m_CloseCallback = CloseRequest;
        params.m_CloseCallbackUserData = &events;
        params.m_FocusCallback = Focus;
        params.m_FocusCallbackUserData = &events;
        params.m_IconifyCallback = Minimize;
        params.m_IconifyCallbackUserData = &events;
    }

    void TearDown()
    {
        dmPlatform::CloseWindow(window);
        dmPlatform::DeleteWindow(window);
    }

    bool WaitFor(int* observed, int value)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
        while (*observed != value && dmTime::GetMonotonicTime() < deadline)
        {
            dmPlatform::PollEvents(window);
        }

        return *observed == value;
    }

};

// Opening and reopening must show the window immediately and deliver focus after OpenWindow returns.
TEST_F(CocoaWindowTest, OpenVisibleWindow)
{
    @autoreleasepool
    {
        params.m_Hidden = false;
        for (int i = 0; i < 2; ++i)
        {
            events.m_Focus = -1;
            ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(window, &params));
            EXPECT_EQ(-1, events.m_Focus);
            NSWindow* native = dmPlatform::GetOSXNSWindow(window);
            ASSERT_TRUE(native.visible);
            ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
            dmPlatform::CloseWindow(window);
        }
    }
}

// A secondary game window must receive startup focus when only the main window is polled.
TEST_F(CocoaWindowTest, PollSecondaryWindow)
{
    @autoreleasepool
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        HWindow second = dmPlatform::NewWindow();
        WindowEvents secondary = {};
        params.m_FocusCallbackUserData = &secondary;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(second, params));
        dmPlatform::ShowWindow(second);
        EXPECT_EQ(0, secondary.m_FocusCount);
        EXPECT_TRUE(WaitFor(&secondary.m_Focus, 1));
        EXPECT_EQ(1, secondary.m_FocusCount);
        dmPlatform::DeleteWindow(second);
    }
}

// If another window becomes key before the rebooted engine polls, both focus changes must reach the game.
TEST_F(CocoaWindowTest, FocusLostBeforeFirstPoll)
{
    @autoreleasepool
    {
        params.m_Hidden = false;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        dmPlatform::CloseWindow(window);

        events.m_FocusCount = 0;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        NSWindow* game = dmPlatform::GetOSXNSWindow(window);
        ASSERT_TRUE(game.keyWindow);
        NSWindow* other = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 120, 80) styleMask:NSWindowStyleMaskTitled
                            backing:NSBackingStoreBuffered defer:NO] autorelease];
        [other setReleasedWhenClosed:NO];
        [other makeKeyAndOrderFront:nil];
        EXPECT_FALSE(game.keyWindow);
        EXPECT_EQ(0, events.m_FocusCount);
        dmPlatform::PollEvents(window);
        EXPECT_EQ(2, events.m_FocusCount);
        EXPECT_EQ(0, events.m_Focus);
        [other close];
    }
}

// Closing and quitting must let the engine finish the frame, and the reopened fullscreen window must still minimize.
TEST_F(CocoaWindowTest, HiddenFocusMinimizeQuitAndReopen)
{
    @autoreleasepool
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        ASSERT_FALSE(native.visible);
        ASSERT_FALSE(native.keyWindow);
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        dmPlatform::IconifyWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Minimized, 1));
        [native deminiaturize:nil];
        ASSERT_TRUE(WaitFor(&events.m_Minimized, 0));
        [native performClose:nil];
        ASSERT_EQ(1, events.m_Closes);
        ASSERT_TRUE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));
        ASSERT_EQ((NSInteger)NSTerminateCancel, (NSInteger)[NSApp.delegate applicationShouldTerminate:NSApp]);
        ASSERT_EQ(2, events.m_Closes);
        events.m_AllowClose = true;
        [native performClose:nil];
        ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));
        ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, dmPlatform::GetNativeWindowId(window).m_Type);
        params.m_Fullscreen = true;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        native = dmPlatform::GetOSXNSWindow(window);
        ASSERT_FALSE(native.styleMask & NSWindowStyleMaskTitled);
        ASSERT_TRUE(NSEqualRects(native.frame, [NSScreen screens].firstObject.frame));
        dmPlatform::ShowWindow(window);
        WindowIconify(window);
        ASSERT_TRUE(WaitFor(&events.m_Minimized, 1));
        ASSERT_TRUE(native.miniaturized);
    }
}

// The Window menu must route Control-Command-F to AppKit's native fullscreen toggle.
TEST_F(CocoaWindowTest, FullscreenShortcut)
{
    @autoreleasepool
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        NSEvent* shortcut = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagControl | NSEventModifierFlagCommand
                             timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:native.windowNumber context:nil
                             characters:@"f" charactersIgnoringModifiers:@"f" isARepeat:NO keyCode:kVK_ANSI_F];
        __block int fullscreen = 0;
        NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
        id enter = [center addObserverForName:NSWindowDidEnterFullScreenNotification object:native queue:nil usingBlock:^(NSNotification*) {
                        fullscreen = 1;
                    }];
        id exit = [center addObserverForName:NSWindowDidExitFullScreenNotification object:native queue:nil usingBlock:^(NSNotification*) {
                       fullscreen = 0;
                   }];
        [NSApp sendEvent:shortcut];
        bool entered = WaitFor(&fullscreen, 1);
        if (entered)
        {
            [NSApp sendEvent:shortcut];
        }

        bool exited = entered && WaitFor(&fullscreen, 0);
        [center removeObserver:enter];
        [center removeObserver:exit];
        ASSERT_TRUE(entered);
        ASSERT_TRUE(exited);
    }
}

// An unavailable OpenGL profile must release the partially opened window so the same handle can be used again.
TEST_F(CocoaWindowTest, FailedOpenThenReopen)
{
    @autoreleasepool
    {
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        params.m_GraphicsApiVersionHint = 46;
        ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, dmPlatform::OpenWindow(window, params));
        ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, dmPlatform::GetNativeWindowId(window).m_Type);
        ASSERT_TRUE(dmPlatform::GetOSXNSWindow(window) == nil);
        ASSERT_TRUE(dmPlatform::GetOSXNSView(window) == nil);
        params.m_GraphicsApiVersionHint = 33;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_COCOA, dmPlatform::GetNativeWindowId(window).m_Type);
    }
}

// OpenGL functions are available with a current context and unavailable on Vulkan or after unbinding.
TEST_F(CocoaWindowTest, OpenGLProcAddressRequiresContext)
{
    @autoreleasepool
    {
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        EXPECT_EQ((uintptr_t)0, dmPlatform::GetProcAddress(window, "glGetString"));
        dmPlatform::CloseWindow(window);

        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        EXPECT_NE((uintptr_t)0, dmPlatform::GetProcAddress(window, "glGetString"));
        [NSOpenGLContext clearCurrentContext];
        EXPECT_EQ((uintptr_t)0, dmPlatform::GetProcAddress(window, "glGetString"));
    }
}

// A move notification alone must update the OpenGL drawable when the window stays on the same display.
TEST_F(CocoaWindowTest, MoveUpdatesOpenGLContext)
{
    @autoreleasepool
    {
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        NSOpenGLContext* original = window->m_Context;
        DMTestOpenGLContext* context = [[[DMTestOpenGLContext alloc] initWithFormat:original.pixelFormat shareContext:nil] autorelease];
        ASSERT_TRUE(context);
        window->m_Context = context;
        context.updated = NO;
        NSWindow* native = window->m_Window;
        [native.delegate windowDidMove:[NSNotification notificationWithName:NSWindowDidMoveNotification object:native]];
        window->m_Context = original;
        ASSERT_TRUE(context.updated);
    }
}

// Moving between displays must update drawable pixels and deliver a resize while keeping the client size in points.
TEST_F(CocoaWindowTest, ChangeBackingScale)
{
    @autoreleasepool
    {
        NSScreen* primary = NSScreen.screens.firstObject;
        NSScreen* secondary = nil;
        for (NSScreen* screen in NSScreen.screens)
        {
            if (screen.backingScaleFactor != primary.backingScaleFactor)
            {
                secondary = screen;
            }
        }

        if (!secondary)
        {
            SKIP();
        }

        params.m_Width = 305;
        params.m_Height = 209;
        params.m_ResizeCallback = Resize;
        params.m_ResizeCallbackUserData = &events;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::ShowWindow(window);
        dmPlatform::SetWindowSize(window, 305, 209);
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        NSScreen* target = native.screen.backingScaleFactor == primary.backingScaleFactor ? secondary : primary;
        for (int i = 0; i < 2; ++i)
        {
            int x = NSMinX(target.visibleFrame) + 50;
            int y = NSMaxY(primary.frame) - NSMaxY(target.visibleFrame) + 70;
            dmPlatform::SetWindowPosition(window, x, y);
            uint32_t width = 305 * target.backingScaleFactor;
            uint32_t height = 209 * target.backingScaleFactor;
            uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
            while ((events.m_DrawableWidth != width || events.m_DrawableHeight != height) && dmTime::GetMonotonicTime() < deadline)
            {
                dmPlatform::PollEvents(window);
            }

            ASSERT_EQ(width, events.m_DrawableWidth);
            ASSERT_EQ(height, events.m_DrawableHeight);
            ASSERT_EQ(width, dmPlatform::GetWindowWidth(window));
            ASSERT_EQ(height, dmPlatform::GetWindowHeight(window));
            uint32_t client_width, client_height;
            ASSERT_TRUE(dmPlatform::GetWindowClientSize(window, &client_width, &client_height));
            ASSERT_EQ(305u, client_width);
            ASSERT_EQ(209u, client_height);
            target = target == primary ? secondary : primary;
        }
    }
}

// Standalone launches restore the saved top-left position and honor the newly requested content size.
TEST_F(CocoaWindowTest, StandaloneFrameAutosave)
{
    @autoreleasepool
    {
        if (getenv("DM_WINDOW_STATE"))
        {
            SKIP();
        }

        NSString* title = [@"Defold placement test " stringByAppendingString:NSUUID.UUID.UUIDString];
        params.m_Title = title.UTF8String;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::ShowWindow(window);
        dmPlatform::SetWindowSize(window, 381, 247);
        dmPlatform::SetWindowPosition(window, 85, 111);
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        NSRect saved = native.frame;
        ASSERT_TRUE([native.frameAutosaveName isEqualToString:title]);
        dmPlatform::CloseWindow(window);
        params.m_Width = 420;
        params.m_Height = 280;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        native = dmPlatform::GetOSXNSWindow(window);
        NSRect restored = native.frame;
        uint32_t width, height;
        ASSERT_TRUE(dmPlatform::GetWindowClientSize(window, &width, &height));
        dmPlatform::CloseWindow(window);
        [NSWindow removeFrameUsingName:title];
        ASSERT_EQ(NSMinX(saved), NSMinX(restored));
        ASSERT_EQ(NSMaxY(saved), NSMaxY(restored));
        ASSERT_EQ(params.m_Width, width);
        ASSERT_EQ(params.m_Height, height);
    }
}

@interface DMTestAutoreleaseWindow : NSWindow
@property (assign) NSHashTable* temporaries;
@end
@implementation DMTestAutoreleaseWindow
- (NSScreen*)screen
{
    [self.temporaries addObject:[[[NSObject alloc] init] autorelease]];
    return [super screen];
}

- (void)setTitle:(NSString*)title
{
    [self.temporaries addObject:title];
}

- (void)setContentSize:(NSSize)size
{
    [self.temporaries addObject:[[[NSObject alloc] init] autorelease]];
    [super setContentSize:size];
}

- (void)setFrameOrigin:(NSPoint)point
{
    [self.temporaries addObject:[[[NSObject alloc] init] autorelease]];
    [super setFrameOrigin:point];
}

@end

// Querying density between event polls must release Cocoa temporaries before returning to the game.
TEST_F(CocoaWindowTest, DisplayDensityReleasesTemporaries)
{
    @autoreleasepool
    {
        DMTestAutoreleaseWindow* native = [[[DMTestAutoreleaseWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES] autorelease];
        NSHashTable* temporaries = [NSHashTable weakObjectsHashTable];
        native.temporaries = temporaries;
        dmWindow handle = {};
        handle.m_Window = native;
        float density = dmPlatform::GetDisplayDensity(&handle);
        native.temporaries = nil;
        ASSERT_GT(density, 0.0f);
        ASSERT_EQ((NSUInteger)0, temporaries.allObjects.count);
    }
}

// A game can set the title between event polls. The temporary string must be released before returning.
TEST_F(CocoaWindowTest, SetWindowTitleReleasesTemporaryString)
{
    @autoreleasepool
    {
        DMTestAutoreleaseWindow* native = [[[DMTestAutoreleaseWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES] autorelease];
        NSHashTable* temporaries = [NSHashTable weakObjectsHashTable];
        native.temporaries = temporaries;
        dmWindow handle = {};
        handle.m_Window = native;
        dmPlatform::SetWindowTitle(&handle, "A dynamically allocated window title");
        native.temporaries = nil;
        ASSERT_EQ((NSUInteger)0, temporaries.allObjects.count);
    }
}

// Scripted geometry changes between event polls must drain AppKit's autoreleased objects.
TEST_F(CocoaWindowTest, SettingWindowGeometryReleasesTemporaries)
{
    @autoreleasepool
    {
        DMTestAutoreleaseWindow* native = [[[DMTestAutoreleaseWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES] autorelease];
        NSHashTable* temporaries = [NSHashTable weakObjectsHashTable];
        native.temporaries = temporaries;
        dmWindow handle = {};
        handle.m_Window = native;
        handle.m_View = native.contentView;
        dmPlatform::SetWindowPosition(&handle, 12, 34);
        ASSERT_EQ((NSUInteger)0, temporaries.allObjects.count);
        dmPlatform::SetWindowSize(&handle, 120, 80);
        ASSERT_EQ((NSUInteger)0, temporaries.allObjects.count);
        native.temporaries = nil;
    }
}

static NSEvent* KeyEvent(NSWindow* window, NSEventType type, unsigned short code, NSEventModifierFlags flags)
{
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags timestamp:0
            windowNumber:window.windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:code];
}

// AppKit focus-loss listeners can read held inputs in both the startup queue and normal dispatch.
TEST_F(CocoaWindowTest, FocusLossReportsPressedInput)
{
    @autoreleasepool
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        dmPlatform::CloseWindow(window);
        params.m_FocusCallback = dmWindowTest::FocusLossInput::Record;
        for (int poll_first = 0; poll_first <= 1; ++poll_first)
        {
            dmWindowTest::FocusLossInput observed = { window, 0, 0, 0 };
            params.m_FocusCallbackUserData = &observed;
            ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
            dmPlatform::ShowWindow(window);
            NSWindow* native = dmPlatform::GetOSXNSWindow(window);
            ASSERT_TRUE(native.keyWindow);
            if (poll_first)
            {
                dmPlatform::PollEvents(window);
            }

            [native.contentView keyDown:KeyEvent(native, NSEventTypeKeyDown, kVK_ANSI_A, 0)];
            [native.contentView mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSZeroPoint
                                           modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil
                                           eventNumber:1 clickCount:1 pressure:1]];
            dmPlatform::HideWindow(window);
            EXPECT_EQ(poll_first, observed.m_Losses);
            EXPECT_EQ(0, dmPlatform::GetKey(window, 'A'));
            EXPECT_EQ(0, dmPlatform::GetMouseButton(window, 0));
            dmPlatform::PollEvents(window);
            EXPECT_EQ(1, observed.m_Losses);
            EXPECT_EQ(1, observed.m_Key);
            EXPECT_EQ(1, observed.m_Button);
            dmPlatform::CloseWindow(window);
        }
    }
}

// Input reports modifier sides and Unicode text, clears marked text on blur and preserves locked pointer deltas.
TEST_F(CocoaWindowTest, TextModifiersAndPointer)
{
    @autoreleasepool
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        NSView<NSTextInputClient>* view = (NSView<NSTextInputClient>*)native.contentView;
        dmPlatform::SetKeyboardCharCallback(window, Character, &events);
        dmPlatform::SetKeyboardMarkedTextCallback(window, Marked, &events);
        NSRange empty = NSMakeRange(NSNotFound, 0);
        [view setMarkedText:@"かな" selectedRange:NSMakeRange(1, 0) replacementRange:empty];
        ASSERT_STREQ("かな", events.m_Marked);
        ASSERT_EQ(2u, (unsigned)view.markedRange.length);
        [view insertText:@"日🌍" replacementRange:empty];
        ASSERT_EQ(2, events.m_Count);
        ASSERT_EQ(0x65e5, events.m_Characters[0]);
        ASSERT_EQ(0x1f30d, events.m_Characters[1]);
        ASSERT_STREQ("", events.m_Marked);
        [view setMarkedText:@"x" selectedRange:NSMakeRange(0, 1) replacementRange:empty];
        [view unmarkText];
        ASSERT_FALSE(view.hasMarkedText);
        ASSERT_EQ(4, events.m_MarkedUpdates);
        [view flagsChanged:KeyEvent(native, NSEventTypeFlagsChanged, kVK_Shift, NSEventModifierFlagShift)];
        [view flagsChanged:KeyEvent(native, NSEventTypeFlagsChanged, kVK_RightShift, NSEventModifierFlagShift)];
        [view flagsChanged:KeyEvent(native, NSEventTypeFlagsChanged, kVK_Shift, NSEventModifierFlagShift)];
        ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSHIFT));
        ASSERT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
        [view flagsChanged:KeyEvent(native, NSEventTypeFlagsChanged, kVK_RightShift, 0)];
        ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
        DMTestPointerEvent* pointer = [[[DMTestPointerEvent alloc] init] autorelease];
        [view scrollWheel:pointer];
        [view scrollWheel:pointer];
        [view scrollWheel:pointer];
        ASSERT_EQ(2, dmPlatform::GetMouseWheel(window));
        NSPoint savedCursor = [NSEvent mouseLocation];
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
        NSPoint centeredCursor = [NSEvent mouseLocation];
        pointer.movement = NSMakePoint(7 + centeredCursor.x - savedCursor.x, -5 + savedCursor.y - centeredCursor.y);
        ASSERT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR_LOCK));
        int32_t x0, y0, x1, y1;
        dmPlatform::GetMousePosition(window, &x0, &y0);
        [view mouseMoved:pointer];
        dmPlatform::GetMousePosition(window, &x1, &y1);
        ASSERT_EQ((int)(7 * native.backingScaleFactor), x1 - x0);
        ASSERT_EQ((int)(-5 * native.backingScaleFactor), y1 - y0);
        [view setMarkedText:@"x" selectedRange:NSMakeRange(0, 1) replacementRange:empty];
        [native orderOut:nil];
        ASSERT_TRUE(WaitFor(&events.m_Focus, 0));
        ASSERT_FALSE(view.hasMarkedText);
        ASSERT_STREQ("", events.m_Marked);
        dmPlatform::GetMousePosition(window, &x0, &y0);
        ASSERT_EQ(x1, x0);
        ASSERT_EQ(y1, y0);
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        dmPlatform::GetMousePosition(window, &x0, &y0);
        ASSERT_EQ(x1, x0);
        ASSERT_EQ(y1, y0);
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, true);
        ASSERT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR_LOCK));

        dmPlatform::CloseWindow(window);
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        view = (NSView<NSTextInputClient>*)dmPlatform::GetOSXNSView(window);
        [view setMarkedText:@"y" selectedRange:NSMakeRange(0, 1) replacementRange:empty];
        ASSERT_STREQ("y", events.m_Marked);
        [view insertText:@"z" replacementRange:empty];
        ASSERT_EQ(3, events.m_Count);
        ASSERT_EQ('z', events.m_Characters[2]);
    }
}

static NSPoint ContentCenter(NSWindow* window)
{
    NSRect content = [window contentRectForFrameRect:window.frame];
    return NSMakePoint(NSMidX(content), NSMidY(content));
}

// Moving and resizing a locked window must preserve virtual motion and restore its original content-relative position.
TEST_F(CocoaWindowTest, LockedCursorFollowsWindow)
{
    @autoreleasepool
    {
        params.m_Title = "";
        params.m_Width = 200;
        params.m_Height = 120;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        dmPlatform::SetWindowPosition(window, 50, 80);
        dmPlatform::ShowWindow(window);
        ASSERT_TRUE(WaitFor(&events.m_Focus, 1));
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        NSPoint original = [NSEvent mouseLocation];
        CGFloat screenTop = NSMaxY([NSScreen screens].firstObject.frame);
        NSRect content = [native contentRectForFrameRect:native.frame];
        NSPoint saved = NSMakePoint(NSMinX(content) + 40, NSMaxY(content) - 30);
        ASSERT_EQ(kCGErrorSuccess, CGWarpMouseCursorPosition(CGPointMake(saved.x, screenTop - saved.y)));
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
        int32_t x0, y0, x1, y1;
        dmPlatform::GetMousePosition(window, &x0, &y0);
        dmPlatform::SetWindowPosition(window, 300, 110);
        ASSERT_TRUE(NSEqualPoints(ContentCenter(native), [NSEvent mouseLocation]));
        dmPlatform::SetWindowSize(window, 160, 100);
        NSPoint center = [NSEvent mouseLocation];
        ASSERT_TRUE(NSEqualPoints(ContentCenter(native), center));
        DMTestPointerEvent* pointer = [[[DMTestPointerEvent alloc] init] autorelease];
        pointer.movement = NSMakePoint(center.x - saved.x, saved.y - center.y);
        [native.contentView mouseMoved:pointer];
        dmPlatform::GetMousePosition(window, &x1, &y1);
        ASSERT_EQ(x0, x1);
        ASSERT_EQ(y0, y1);
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, true);
        NSPoint restored = [NSEvent mouseLocation];
        CGWarpMouseCursorPosition(CGPointMake(original.x, screenTop - original.y));
        content = [native contentRectForFrameRect:native.frame];
        // Quartz can round a fractional mouse location when warping it back.
        ASSERT_NEAR(40.0, restored.x - NSMinX(content), 1.0);
        ASSERT_NEAR(30.0, NSMaxY(content) - restored.y, 1.0);
    }
}

struct WorkerUpload
{
    HWindow window;
    GLuint  texture;
};
static void* Upload(void* data)
{
    @autoreleasepool
    {
        WorkerUpload* upload = (WorkerUpload*)data;
        void* context = dmPlatform::AcquireAuxContext(upload->window);
        if (context)
        {
            glGenTextures(1, &upload->texture);
            glBindTexture(GL_TEXTURE_2D, upload->texture);
            const GLubyte pixel[] = { 3, 147, 255, 255 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            glFinish();
            dmPlatform::UnacquireAuxContext(upload->window, context);
        }
    }
    return 0;
}

// A hidden OpenGL window must still wait for refresh intervals when vsync is enabled.
TEST_F(CocoaWindowTest, OccludedOpenGLSwapInterval)
{
    @autoreleasepool
    {
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        NSWindow* native = dmPlatform::GetOSXNSWindow(window);
        ASSERT_FALSE(native.occlusionState & NSWindowOcclusionStateVisible);
        dmPlatform::SetSwapInterval(window, 1);
        NSInteger refresh_rate = native.screen.maximumFramesPerSecond;
        uint64_t period = 1000000 / (refresh_rate > 0 ? refresh_rate : 60);
        uint64_t start = dmTime::GetMonotonicTime();
        for (int i = 0; i < 6; ++i)
        {
            dmPlatform::SwapBuffers(window);
        }

        // The first swap can fall anywhere within a refresh period.
        ASSERT_GE(dmTime::GetMonotonicTime() - start, 5 * period);
    }
}

// A worker upload must be visible in the main OpenGL context after the worker releases its context.
TEST_F(CocoaWindowTest, SharedOpenGLUpload)
{
    @autoreleasepool
    {
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        ASSERT_TRUE(dmPlatform::GetProcAddress(window, "glTexImage2D") != 0);
        ASSERT_TRUE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_AUX_CONTEXT));
        WorkerUpload upload = { window, 0 };
        pthread_t worker;
        ASSERT_EQ(0, pthread_create(&worker, 0, Upload, &upload));
        ASSERT_EQ(0, pthread_join(worker, 0));
        ASSERT_TRUE(glIsTexture(upload.texture));
        glBindTexture(GL_TEXTURE_2D, upload.texture);
        GLubyte pixel[4] = {};
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        ASSERT_EQ(147u, (unsigned)pixel[1]);
        glDeleteTextures(1, &upload.texture);
        dmPlatform::SetSwapInterval(window, 0);
        dmPlatform::SwapBuffers(window);
    }
}
