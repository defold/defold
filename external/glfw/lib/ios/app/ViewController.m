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

#import "ViewController.h"
#include "../../../include/GL/glfw.h"
#include "internal.h"

#import "VulkanView.h"
#import "EAGLView.h"

extern int g_IsReboot;
static int g_view_type = GLFW_NO_API;

@implementation ViewController

@synthesize baseView;

- (void)dealloc
{
    [baseView release];
    [super dealloc];
}

- (void)createView:(BOOL)recreate
{
    CGRect bounds = self.view.bounds;
    cachedViewSize = bounds.size;

    if (g_view_type == GLFW_NO_API)
    {
        baseView = [VulkanView createView: bounds recreate:recreate];
    }
    else
    {
        baseView = [EAGLView createView: bounds recreate:recreate];
    }

    [[self view] removeFromSuperview];

    [[self view] insertSubview:baseView atIndex:0];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizesSubviews = YES;

    _glfwWin.viewController = self;

    [self createView:FALSE];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];

    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        // According to Apple glFinish() should be called here (Comment moved from AppDelegate applicationWillResignActive)
        glFinish();
    }
}

- (BOOL)shouldUpdateViewFrame
{
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation orientation = device.orientation;
    bool update_parent_frame = false;
    switch (orientation) {
        case UIDeviceOrientationLandscapeLeft:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationLandscapeRight:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationPortrait:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            update_parent_frame = true;
            break;
        default:
            break;
    }
    return update_parent_frame;
}

- (CGSize)getIntendedViewSize
{
    CGSize result;
    CGRect parent_bounds = self.view.bounds;

    if (0 != g_IsReboot) {
        parent_bounds.size = cachedViewSize;
    }
    bool flipBounds = false;
    if (_glfwWin.portrait) {
        flipBounds = parent_bounds.size.width > parent_bounds.size.height;
    } else {
        flipBounds = parent_bounds.size.width < parent_bounds.size.height;
    }
    if (flipBounds) {
        result.width = parent_bounds.size.height;
        result.height = parent_bounds.size.width;
    } else {
        result = parent_bounds.size;
    }
    return result;
}

- (CGPoint)getIntendedFrameOrigin:(CGSize)size
{
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation orientation = device.orientation;
    CGPoint origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    switch (orientation) {
        case UIDeviceOrientationLandscapeLeft:
            break;
        case UIDeviceOrientationLandscapeRight:
            origin.x = -(size.width - size.height);
            origin.y = size.width - size.height;

            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.x = 0.0f;
                origin.y = 0.0f;
            }
            break;
        case UIDeviceOrientationPortrait:
            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.x = -(size.width - size.height);
            }
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.y = (size.width - size.height);
            }
            break;
        default:
            break;
    }
    return origin;
}

- (void)viewDidAppear:(BOOL)animated
{
    [baseView setCurrentContext];

    [super viewDidAppear: animated];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
}

#pragma mark UIContentContainer

- (void)accelerometer:(UIAccelerometer *)accelerometer didAccelerate:(UIAcceleration *)acceleration
{
    _glfwInput.AccX = acceleration.x;
    _glfwInput.AccY = acceleration.y;
    _glfwInput.AccZ = acceleration.z;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

@end

void _glfwPlatformSetViewType(int view_type)
{
    g_view_type = view_type;
}

void _glfwPlatformSetWindowBackgroundColor(unsigned int color)
{
    float r = (color & 0xff) / 255.0f;
    float g = ((color >> 8) & 0xff) / 255.0f;
    float b = ((color >> 16) & 0xff) / 255.0f;
    float a = 1.0f;
    UIViewController *viewController = (UIViewController*)_glfwWin.viewController;
    viewController.view.backgroundColor = [[UIColor alloc]initWithRed:r green:g blue:b alpha:a];
}

float _glfwPlatformGetDisplayScaleFactor()
{
    return 1.0f;
}

void* _glfwPlatformAcquireAuxContext()
{
    if (_glfwWin.clientAPI == GLFW_NO_API)
    {
        _glfwPlatformAcquireAuxContextVulkan();
    }
    else
    {
        _glfwPlatformAcquireAuxContextOpenGL();
    }
    return 0;
}

void _glfwPlatformUnacquireAuxContext(void* context)
{
    if (_glfwWin.clientAPI == GLFW_NO_API)
    {
        _glfwPlatformUnacquireAuxContextVulkan(context);
    }
    else
    {
        _glfwPlatformUnacquireAuxContextOpenGL(context);
    }
}

int _glfwPlatformQueryAuxContext()
{
    if (_glfwWin.clientAPI == GLFW_NO_API)
    {
        return _glfwPlatformQueryAuxContextVulkan();
    }
    else
    {
        return _glfwPlatformQueryAuxContextOpenGL();
    }
}

int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    if (wndconfig->clientAPI == GLFW_NO_API)
    {
        return _glfwPlatformOpenWindowVulkan(width, height, wndconfig, fbconfig);
    }
    else
    {
        return _glfwPlatformOpenWindowOpenGL(width, height, wndconfig, fbconfig);
    }
}
