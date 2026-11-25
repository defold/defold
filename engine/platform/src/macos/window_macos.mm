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
#import <QuartzCore/CAMetalLayer.h>
#include "../platform_window_osx.h"
#include <dlib/log.h>
#include <string.h>

@interface DMGameWindow : NSWindow
@end
@implementation DMGameWindow
- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}

@end

@interface DMWindowDelegate : NSObject <NSWindowDelegate>
{
    @public
    HWindow handle;
}
@end

@implementation DMWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    if (dmPlatform::TriggerCloseCallback(handle))
    {
        dmPlatform::CloseWindow(handle);
    }

    return NO;
}

- (void)windowDidMove:(NSNotification*)notification
{
    // Moving the custom content view changes the OpenGL drawable's geometry even on the same display.
    [handle->m_Context update];
    dmPlatform::CenterLockedCursor(handle);
    dmPlatform::WindowPlacementChanged(handle);
}

- (void)windowDidResize:(NSNotification*)notification
{
    dmPlatform::CenterLockedCursor(handle);
    dmPlatform::UpdateWindowMetrics(handle, true);
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    dmPlatform::UpdateWindowMetrics(handle, true);
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
    dmPlatform::UpdateWindowMetrics(handle, true);
}

- (void)setFocused:(BOOL)focused
{
    if (handle->m_Params.m_Fullscreen)
    {
        handle->m_Window.level = focused ? NSMainMenuWindowLevel + 1 : NSNormalWindowLevel;
    }

    dmPlatform::UpdateCursorMode(handle, focused);
    if (!focused)
    {
        NSView<NSTextInputClient>* view = (NSView<NSTextInputClient>*)handle->m_View;
        if (view.hasMarkedText)
        {
            [view.inputContext discardMarkedText];
            if (view.hasMarkedText)
            {
                [view unmarkText];
            }
        }

        memset(handle->m_Keys, 0, sizeof(handle->m_Keys));
        memset(handle->m_Buttons, 0, sizeof(handle->m_Buttons));
    }

    dmPlatform::WindowFocusChanged(handle->m_Focus, handle->m_Params, focused);
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    [self setFocused:YES];
}

- (void)windowDidResignKey:(NSNotification*)notification {
    [self setFocused:NO];
}

- (void)setMinimized:(BOOL)minimized
{
    if (handle->m_Ready && handle->m_Params.m_IconifyCallback)
    {
        handle->m_Params.m_IconifyCallback(handle->m_Params.m_IconifyCallbackUserData, minimized);
    }
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    [self setMinimized:YES];
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    [self setMinimized:NO];
}

@end

@interface DMGameApplicationDelegate : NSObject <NSApplicationDelegate>
@end
@implementation DMGameApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    // The engine's close callback requests orderly shutdown after the current frame.
    for (NSWindow* window in [sender windows])
    {
        if ([window.delegate isKindOfClass:[DMWindowDelegate class]])
        {
            [window.delegate windowShouldClose:window];
        }
    }

    return NSTerminateCancel;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
    return YES;
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    dmPlatform::WindowPlacementMonitorsChanged();
}

@end

namespace dmPlatform
{
    static unsigned g_WindowCount;
    static bool g_Launched;
    static DMGameApplicationDelegate* g_Delegate;
    static id g_CommandKeyMonitor;
    static NSArray* g_MenuNibObjects;

    static NSString* ApplicationName(NSBundle* bundle)
    {
        for (NSString* key in @[@"CFBundleDisplayName", @"CFBundleName"])
        {
            NSString* name = [bundle objectForInfoDictionaryKey:key];
            if ([name isKindOfClass:[NSString class]] && name.length)
            {
                return name;
            }
        }

        return NSProcessInfo.processInfo.processName;
    }

    static void InstallMenus()
    {
        if (NSApp.mainMenu)
        {
            return;
        }

        NSBundle* bundle = NSBundle.mainBundle;
        NSArray* objects = nil;
        if ([bundle URLForResource:@"MainMenu" withExtension:@"nib"] &&
            [bundle loadNibNamed:@"MainMenu" owner:NSApp topLevelObjects:&objects] && NSApp.mainMenu)
        {
            // Menu items do not retain their targets. The application menu survives window reopens.
            [g_MenuNibObjects release];
            g_MenuNibObjects = [objects retain];
            return;
        }

        NSString* name = ApplicationName(bundle);
        NSMenu* main = [[[NSMenu alloc] init] autorelease];
        NSMenu* app = [[[NSMenu alloc] initWithTitle:name] autorelease];
        [main addItemWithTitle:name action:nil keyEquivalent:@""].submenu = app;
        [app addItemWithTitle:[@"About " stringByAppendingString:name] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
        [app addItem:[NSMenuItem separatorItem]];
        NSMenu* services = [[[NSMenu alloc] initWithTitle:@"Services"] autorelease];
        [app addItemWithTitle:@"Services" action:nil keyEquivalent:@""].submenu = services;
        NSApp.servicesMenu = services;
        [app addItem:[NSMenuItem separatorItem]];
        [app addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
        [app addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"].keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
        [app addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
        [app addItem:[NSMenuItem separatorItem]];
        [app addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];
        NSMenu* windows = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
        [main addItemWithTitle:@"Window" action:nil keyEquivalent:@""].submenu = windows;
        [windows addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [windows addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
        [windows addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
        [windows addItem:[NSMenuItem separatorItem]];
        [windows addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"].keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;
        NSApp.windowsMenu = windows;
        NSApp.mainMenu = main;
    }

    HWindow NewWindow()
    {
        @autoreleasepool
        {
            if (![NSThread isMainThread])
            {
                dmLogError("Cannot create a macOS window off the main thread.");
                return 0;
            }

            if (g_WindowCount++ == 0)
            {
                [NSApplication sharedApplication];
                if (!NSApp.delegate)
                {
                    g_Delegate = [[DMGameApplicationDelegate alloc] init];
                    NSApp.delegate = g_Delegate;
                }

                [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
                InstallMenus();
                if (!g_Launched)
                {
                    if (!NSApp.running)
                    {
                        [NSApp finishLaunching];
                    }

                    g_Launched = true;
                }

                // AppKit can consume a Command-key release before the content view receives it.
                g_CommandKeyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:^NSEvent*(NSEvent* event) {
                                           if ((event.modifierFlags & NSEventModifierFlagCommand) && [NSApp.keyWindow.delegate isKindOfClass:[DMWindowDelegate class]])
                                           {
                                               [NSApp.keyWindow sendEvent:event];
                                               return nil;
                                           }

                                           return event;
                                       }];
            }

            return new dmWindow();
        }
    }

    void DeleteWindow(HWindow window)
    {
        if (window->m_Window)
        {
            CloseWindow(window);
        }

        SetGamepadEventCallback(window, 0, 0);
        delete window;
        if (--g_WindowCount == 0)
        {
            ShutdownJoysticks();
            if (g_CommandKeyMonitor)
            {
                [NSEvent removeMonitor:g_CommandKeyMonitor];
            }

            g_CommandKeyMonitor = nil;
            if (NSApp.delegate == g_Delegate)
            {
                NSApp.delegate = nil;
            }

            [g_Delegate release];
            g_Delegate = nil;
        }
    }

    void UpdateWindowMetrics(HWindow window, bool notify)
    {
        NSRect bounds = window->m_View.bounds;
        NSSize drawable = [window->m_View convertRectToBacking:bounds].size;
        CGFloat scale = window->m_Window.backingScaleFactor;
        bool changed = !NSEqualSizes(drawable, window->m_DrawableSize) ||
                       !NSEqualSizes(bounds.size, window->m_ClientSize) || window->m_Scale != scale;
        window->m_ClientSize = bounds.size;
        window->m_DrawableSize = drawable;
        window->m_Scale = scale;
        if (changed && window->m_View.layer)
        {
            window->m_View.layer.contentsScale = scale;
            if (window->m_Params.m_GraphicsApi == WINDOW_GRAPHICS_API_VULKAN && [window->m_View.layer isKindOfClass:[CAMetalLayer class]])
            {
                [(CAMetalLayer*)window->m_View.layer setDrawableSize:drawable];
            }
        }

        [window->m_Context update];
        if (changed && notify && window->m_Ready)
        {
            WindowPlacementChanged(window);
            NotifyWindowResize(window, window->m_Params, window->m_ResizeCallbackCount);
        }
    }

    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        @autoreleasepool
        {
            if (window->m_Window)
            {
                return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
            }

            if (!params.m_Fullscreen && (params.m_Width == 0 || params.m_Height == 0))
            {
                return WINDOW_RESULT_WINDOW_OPEN_ERROR;
            }

            if (params.m_GraphicsApi != WINDOW_GRAPHICS_API_OPENGL && params.m_GraphicsApi != WINDOW_GRAPHICS_API_METAL &&
                params.m_GraphicsApi != WINDOW_GRAPHICS_API_VULKAN && params.m_GraphicsApi != WINDOW_GRAPHICS_API_WEBGPU)
            {
                return WINDOW_RESULT_WINDOW_OPEN_ERROR;
            }

            window->m_Params = params;
            InitWindowPlacement(window, params);
            NSScreen* screen = [NSScreen screens].firstObject;
            NSRect rect = NSMakeRect(0, 0, params.m_Width, params.m_Height);
            NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
            if (params.m_Fullscreen && screen)
            {
                rect = screen.frame;
                style = NSWindowStyleMaskBorderless | NSWindowStyleMaskMiniaturizable;
            }

            NSWindow* native = [[DMGameWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:NO];
            window->m_Window = native;
            if (!native)
            {
                DeleteWindowPlacement(window);
                return WINDOW_RESULT_WINDOW_OPEN_ERROR;
            }

            native.releasedWhenClosed = NO;
            native.acceptsMouseMovedEvents = YES;
            native.title = params.m_Title ? [NSString stringWithUTF8String:params.m_Title] : @"";
            native.tabbingMode = NSWindowTabbingModeDisallowed;
            window->m_View = NewContentView(window, NSMakeRect(0, 0, rect.size.width, rect.size.height));
            native.contentView = window->m_View;
            [native makeFirstResponder:window->m_View];
            if (params.m_Fullscreen)
            {
                native.hasShadow = NO;
                [native setFrame:rect display:NO];
            }
            else
            {
                [native center];
                if (!window->m_Placement && native.title.length)
                {
                    [native setFrameAutosaveName:native.title];
                    [native setContentSize:rect.size];
                }
            }

            window->m_Delegate = [[DMWindowDelegate alloc] init];
            window->m_Delegate->handle = window;
            native.delegate = window->m_Delegate;
            if (!window->m_View || (params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL && !OpenGLCreateContexts(window)))
            {
                CloseWindow(window);
                return WINDOW_RESULT_WINDOW_OPEN_ERROR;
            }

            if (!params.m_Fullscreen)
            {
                RestoreWindowPlacement(window);
            }

            UpdateWindowMetrics(window, false);
            if (!params.m_Hidden && params.m_FocusOnShow)
            {
                [NSApp activateIgnoringOtherApps:YES];
                [native makeKeyAndOrderFront:nil];
            }

            window->m_Ready = true;
            return WINDOW_RESULT_OK;
        }
    }

    void CloseWindow(HWindow window)
    {
        @autoreleasepool
        {
            if (!window->m_Window)
            {
                return;
            }

            UpdateWindowPlacement(window, true);
            DeleteWindowPlacement(window);
            window->m_Ready = false;
            UpdateCursorMode(window, false);
            window->m_Window.delegate = nil;
            DetachContentView(window->m_View);
            OpenGLDestroyContexts(window);
            [window->m_Window orderOut:nil];
            [window->m_Window close];
            [window->m_Delegate release];
            [window->m_View release];
            [window->m_Window release];
            // Input callbacks belong to the handle and may be installed before opening it.
            WindowInputCallbacks callbacks = window->m_InputCallbacks;
            *window = dmWindow();
            window->m_InputCallbacks = callbacks;
        }
    }

    void ShowWindow(HWindow window)
    {
        if (window->m_Params.m_FocusOnShow)
        {
            [NSApp activateIgnoringOtherApps:YES];
            [window->m_Window makeKeyAndOrderFront:nil];
        }
        else
        {
            [window->m_Window orderFront:nil];
        }

        WindowPlacementShown(window);
    }

    void HideWindow(HWindow window)
    {
        [window->m_Window orderOut:nil];
    }

    void IconifyWindow(HWindow window)
    {
        [window->m_Window miniaturize:nil];
    }

    static HWindow ReadyWindow(NSWindow* native)
    {
        DMWindowDelegate* delegate = (DMWindowDelegate*)native.delegate;
        return [delegate isKindOfClass:[DMWindowDelegate class]] && delegate->handle->m_Ready ? delegate->handle : 0;
    }

    void PollEvents(HWindow)
    {
        @autoreleasepool
        {
            if (![NSThread isMainThread])
            {
                dmLogError("AppKit events must be polled on the main thread.");
                return;
            }

            NSArray* windows = [NSApp windows];
            for (NSWindow* native in windows)
            {
                HWindow window = ReadyWindow(native);
                if (window)
                {
                    PollWindowFocus(window->m_Focus, window->m_Params);
                }
            }

            NSEvent* event;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES]))
            {
                [NSApp sendEvent:event];
            }

            for (NSWindow* native in windows)
            {
                HWindow window = ReadyWindow(native);
                if (window)
                {
                    UpdateWindowPlacement(window, false);
                }
            }
        }
    }

    void SetWindowTitle(HWindow window, const char* title)
    {
        @autoreleasepool
        {
            window->m_Window.title = [NSString stringWithUTF8String:title];
        }
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        @autoreleasepool
        {
            const uint32_t count = window->m_ResizeCallbackCount;
            WindowPlacementSizeRequested(window);
            [window->m_Window setContentSize:NSMakeSize(width, height)];
            ClampWindowPlacement(window);
            UpdateWindowMetrics(window, true);
            if (window->m_Ready && window->m_ResizeCallbackCount == count)
            {
                NotifyWindowResize(window, window->m_Params, window->m_ResizeCallbackCount);
            }
        }
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        @autoreleasepool
        {
            SetCocoaWindowPosition(window, x, y);
            WindowPlacementChanged(window);
        }
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_DrawableSize.width;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_DrawableSize.height;
    }

    bool GetWindowClientSize(HWindow window, uint32_t* width, uint32_t* height)
    {
        if (!window->m_Ready)
        {
            return false;
        }

        *width = window->m_ClientSize.width;
        *height = window->m_ClientSize.height;
        return true;
    }

    bool IsWindowResizeSupported(HWindow window)
    {
        return window->m_Ready && !window->m_Params.m_Fullscreen && !(window->m_Window.styleMask & NSWindowStyleMaskFullScreen);
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return window->m_Scale;
    }

    static CGDirectDisplayID DisplayId(NSScreen* screen)
    {
        return [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
    }

    float GetDisplayDensity(HWindow window)
    {
        @autoreleasepool
        {
            NSScreen* screen = window->m_Window.screen;
            CGDirectDisplayID display = DisplayId(screen);
            CGFloat pixels = NSWidth(screen.frame) * screen.backingScaleFactor;
            return CalculateDisplayDensity(CGDisplayScreenSize(display).width, pixels);
        }
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
            {
                return window->m_Ready;
            }
            case WINDOW_STATE_ACTIVE:
            {
                return window->m_Window.keyWindow;
            }
            case WINDOW_STATE_ICONIFIED:
            {
                return window->m_Window.miniaturized;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return window->m_Params.m_HighDPI;
            }
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                return window->m_Params.m_Samples;
            }
            case WINDOW_STATE_AUX_CONTEXT:
            {
                return window->m_AuxContext != nil;
            }
            case WINDOW_STATE_REFRESH_RATE:
            {
                return window->m_Window.screen.maximumFramesPerSecond;
            }
            default:
            {
                break;
            }
        }

        return 0;
    }

    NativeWindowId GetNativeWindowId(HWindow window)
    {
        NativeWindowId id = {};
        if (window && window->m_Ready && window->m_Window.windowNumber > 0)
        {
            id.m_Type = NATIVE_WINDOW_TYPE_COCOA;
            id.m_Id = window->m_Window.windowNumber;
        }

        return id;
    }

    id GetOSXNSWindow(HWindow window)
    {
        return window->m_Window;
    }

    id GetOSXNSView(HWindow window)
    {
        return window->m_View;
    }

    id GetOSXNSOpenGLContext(HWindow window)
    {
        return window->m_Context;
    }

    bool GetSafeArea(HWindow window, WindowSafeArea* area)
    {
        *area = WindowSafeArea();
        area->m_Width = GetWindowWidth(window);
        area->m_Height = GetWindowHeight(window);
        return true;
    }

    uint32_t GetReservedAreas(HWindow, const WindowReservedArea** areas)
    {
        *areas = 0;
        return 0;
    }

    void GetReservedAreas(HWindow, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
    }

    uint32_t GetTouchData(HWindow, WindowTouchData*, uint32_t)
    {
        return 0;
    }

    bool GetAcceleration(HWindow, float*, float*, float*)
    {
        return false;
    }

    bool IsMouseLeftButtonFromTouch(HWindow)
    {
        return false;
    }

    bool IsMousePositionFromTouch(HWindow)
    {
        return false;
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        return window->m_Params.m_CloseCallback ? window->m_Params.m_CloseCallback(window->m_Params.m_CloseCallbackUserData) : 1;
    }

}
