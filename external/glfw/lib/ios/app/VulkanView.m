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

#include "VulkanView.h"
#import "TextUtil.h"
#import "AppDelegate.h"
#import <QuartzCore/CAMetalLayer.h>

#include "internal.h"

// AppDelegate.m
extern _GLFWwin         g_Savewin;
extern UIWindow*        g_ApplicationWindow;
extern AppDelegate*     g_ApplicationDelegate;

static VulkanView* 		g_VulkanView = 0;

@implementation VulkanView

/** Returns a Metal-compatible layer. */
+(Class) layerClass {
    return [CAMetalLayer class];
}

+ (BaseView*)createView:(CGRect)bounds recreate:(BOOL)recreate
{
    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    g_VulkanView = [[[VulkanView alloc] initWithFrame: bounds] autorelease];
    g_VulkanView.contentScaleFactor = scaleFactor;
    g_VulkanView.layer.contentsScale = scaleFactor;
    return g_VulkanView;
}

// called from initWithFrame
- (void)setupView
{
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

int  _glfwPlatformOpenWindowVulkan( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    CGRect view_bounds = g_VulkanView.bounds;

    [g_VulkanView setWindowWidth:view_bounds.size.width * g_VulkanView.contentScaleFactor];
    [g_VulkanView setWindowHeight:view_bounds.size.height * g_VulkanView.contentScaleFactor];

    _glfwWin.width = [g_VulkanView getWindowWidth];
    _glfwWin.height = [g_VulkanView getWindowHeight];

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;

    _glfwWin.pixelFormat = nil;
    _glfwWin.delegate = g_ApplicationDelegate;

    _glfwWin.view = g_VulkanView;
    _glfwWin.window = g_ApplicationWindow;

    // opengl
    _glfwWin.context = nil;
    _glfwWin.aux_context = nil;

	// vulkan
    _glfwWin.clientAPI = GLFW_NO_API;

    return GL_TRUE;
}
