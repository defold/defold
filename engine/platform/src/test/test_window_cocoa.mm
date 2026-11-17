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
#include <dlib/time.h>
#include "../platform_window_osx.h"
#include "../platform_window_constants.h"

@interface DMTestPointerEvent : NSEvent
@property NSPoint warp;
@end
@implementation DMTestPointerEvent
- (BOOL)hasPreciseScrollingDeltas {
    return YES;
}

- (CGFloat)scrollingDeltaY {
    return 6;
}

- (CGFloat)deltaX {
    return 7 + self.warp.x;
}

- (CGFloat)deltaY {
    return -5 + self.warp.y;
}

@end

struct WindowEvents
{
    int      m_Closes, m_Focus, m_Minimized;
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
    ((WindowEvents*)data)->m_Focus = value;
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

// Closing and quitting must give the engine a chance to finish the frame, including after a reboot.
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
        ASSERT_EQ((NSUInteger)NSWindowStyleMaskBorderless, (NSUInteger)native.styleMask);
        ASSERT_TRUE(NSEqualRects(native.frame, [NSScreen screens].firstObject.frame));
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

// A standalone game reopening the same title must recover its native Cocoa frame.
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
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        native = dmPlatform::GetOSXNSWindow(window);
        bool restored = NSEqualRects(saved, native.frame);
        dmPlatform::CloseWindow(window);
        [NSWindow removeFrameUsingName:title];
        ASSERT_TRUE(restored);
    }
}

static NSEvent* KeyEvent(NSWindow* window, NSEventType type, unsigned short code, NSEventModifierFlags flags)
{
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags timestamp:0
            windowNumber:window.windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:code];
}

// Modifier sides, Unicode composition and accumulated pointer deltas are observable through the window API.
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
        pointer.warp = NSMakePoint(centeredCursor.x - savedCursor.x, savedCursor.y - centeredCursor.y);
        ASSERT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR_LOCK));
        int32_t x0, y0, x1, y1;
        dmPlatform::GetMousePosition(window, &x0, &y0);
        [view mouseMoved:pointer];
        dmPlatform::GetMousePosition(window, &x1, &y1);
        ASSERT_EQ((int)(7 * native.backingScaleFactor), x1 - x0);
        ASSERT_EQ((int)(-5 * native.backingScaleFactor), y1 - y0);
        dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, true);
        ASSERT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR_LOCK));
        [view setMarkedText:@"x" selectedRange:NSMakeRange(0, 1) replacementRange:empty];
        [native orderOut:nil];
        ASSERT_TRUE(WaitFor(&events.m_Focus, 0));
        ASSERT_FALSE(view.hasMarkedText);
        ASSERT_STREQ("", events.m_Marked);

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
