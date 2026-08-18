// Copyright 2020-2023 The Defold Foundation
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

#include "MetalView.h"
#import "TextUtil.h"
#import "AppDelegate.h"
#import <QuartzCore/CAMetalLayer.h>

#include "internal.h"

// AppDelegate.m
extern _GLFWwin         g_Savewin;
extern UIWindow*        g_ApplicationWindow;
extern AppDelegate*     g_ApplicationDelegate;

// ios_window.m
GLFWAPI void* glfwIosGetExternalView(void);
extern UIView* _glfwIosGetActiveView(void);
extern int _glfwIosIsEmbedHost(void);

static MetalView*        g_MetalView = 0;

static CGSize GetDrawableSize(MetalView* view)
{
    CGRect view_bounds = view.bounds;
    CGFloat scale_factor = view.contentScaleFactor;

    if (view_bounds.size.width <= 0.0f || view_bounds.size.height <= 0.0f)
    {
        view_bounds = [[UIScreen mainScreen] bounds];
        scale_factor = [[UIScreen mainScreen] scale];
    }

    return CGSizeMake(view_bounds.size.width * scale_factor, view_bounds.size.height * scale_factor);
}

static void UpdateWindowSize(MetalView* view, BOOL notify)
{
    CGSize drawable_size = GetDrawableSize(view);
    const int width = (int)(drawable_size.width + 0.5f);
    const int height = (int)(drawable_size.height + 0.5f);

    if (width <= 0 || height <= 0)
    {
        return;
    }

    const BOOL changed = _glfwWin.width != width || _glfwWin.height != height;
    [view setWindowWidth:width];
    [view setWindowHeight:height];
    ((CAMetalLayer*)view.layer).drawableSize = drawable_size;
    _glfwWin.width = width;
    _glfwWin.height = height;

    if (notify && changed && _glfwWin.windowSizeCallback)
    {
        _glfwWin.windowSizeCallback(width, height);
    }
}

@implementation MetalView

/** Returns a Metal-compatible layer. */
+(Class) layerClass {
    return [CAMetalLayer class];
}

+ (BaseView*)createView:(CGRect)bounds recreate:(BOOL)recreate
{
    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    g_MetalView = [[[MetalView alloc] initWithFrame: bounds] autorelease];
    g_MetalView.contentScaleFactor = scaleFactor;
    g_MetalView.layer.contentsScale = scaleFactor;
    return g_MetalView;
}

// called from initWithFrame
- (void)setupView
{
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    UpdateWindowSize(self, YES);
}

// called from dealloc
- (void)teardownView
{
}


- (void)swapBuffers
{
}

- (void)dealloc
{
    [super dealloc];
}

@end

void* _glfwPlatformAcquireAuxContextVulkan()
{
    return 0;
}

void _glfwPlatformUnacquireAuxContextVulkan(void* context)
{
}

int _glfwPlatformQueryAuxContextVulkan()
{
    return 0;
}

// Declared in ios_window.m
extern UIView* _glfwIosGetActiveView(void);
extern int _glfwIosIsEmbedHost(void);
GLFWAPI void* glfwIosGetExternalView(void);

static void UpdateExternalViewSize(UIView* view, BOOL notify)
{
    if (!view)
        return;

    CGRect bounds = view.bounds;
    CGFloat scale = view.contentScaleFactor > 0.0 ? view.contentScaleFactor : [[UIScreen mainScreen] scale];
    if (bounds.size.width <= 0.0f || bounds.size.height <= 0.0f)
    {
        bounds = [[UIScreen mainScreen] bounds];
        scale = [[UIScreen mainScreen] scale];
    }

    const int width = (int)(bounds.size.width * scale + 0.5f);
    const int height = (int)(bounds.size.height * scale + 0.5f);
    if (width <= 0 || height <= 0)
        return;

    const BOOL changed = _glfwWin.width != width || _glfwWin.height != height;
    _glfwWin.width = width;
    _glfwWin.height = height;

    if (notify && changed && _glfwWin.windowSizeCallback)
        _glfwWin.windowSizeCallback(width, height);
}

int  _glfwPlatformOpenWindowVulkan( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    UIView* external = (UIView*)glfwIosGetExternalView();
    if (external || _glfwIosIsEmbedHost())
    {
        // Host-driven embed: use injected UIView; do not create MetalView /
        // UIWindow / CADisplayLink (host owns the frame clock).
        UIView* view = external ? external : _glfwIosGetActiveView();
        UpdateExternalViewSize(view, NO);

        _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;
        g_Savewin.portrait = _glfwWin.portrait;

        _glfwWin.pixelFormat = nil;
        _glfwWin.delegate = nil;
        _glfwWin.view = view;
        _glfwWin.window = view.window;
        _glfwWin.context = nil;
        _glfwWin.aux_context = nil;
        _glfwWin.clientAPI = GLFW_NO_API;
        return GL_TRUE;
    }

    UpdateWindowSize(g_MetalView, NO);

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;

    _glfwWin.pixelFormat = nil;
    _glfwWin.delegate = g_ApplicationDelegate;

    _glfwWin.view = g_MetalView;
    _glfwWin.window = g_ApplicationWindow;

    // opengl
    _glfwWin.context = nil;
    _glfwWin.aux_context = nil;

	// no API
    _glfwWin.clientAPI = GLFW_NO_API;

    return GL_TRUE;
}
