#import "ViewController.h"
#include "../../../include/GL/glfw.h"
#include "internal.h"

#import "VulkanView.h"
#import "EAGLView.h"

extern int g_IsReboot;

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
    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && version < 8.1) {
        CGSize size = [self getIntendedViewSize];
        CGRect parent_bounds = self.view.bounds;
        parent_bounds.size = size;

        if ([self shouldUpdateViewFrame]) {
            CGPoint origin = [self getIntendedFrameOrigin: size];

            CGRect parent_frame = self.view.frame;
            parent_frame.origin = origin;
            parent_frame.size = size;

            self.view.frame = parent_frame;
        }
        bounds = parent_bounds;
    }
    cachedViewSize = bounds.size;

    if (_glfwWin.clientAPI == GLFW_NO_API)
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

- (void)updateViewFramesWorkaround
{
    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && version < 8.1) {
        CGRect parent_frame = self.view.frame;
        CGRect parent_bounds = self.view.bounds;

        CGSize size = [self getIntendedViewSize];

        parent_bounds.size = size;

        if ([self shouldUpdateViewFrame]) {
            CGPoint origin = [self getIntendedFrameOrigin: size];
            parent_frame.origin = origin;
            parent_frame.size = size;

            self.view.frame = parent_frame;
        }

        baseView.frame = parent_bounds;
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

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // NOTE: For iOS < 6
    if (_glfwWin.portrait)
    {
        return   interfaceOrientation == UIInterfaceOrientationPortrait
              || interfaceOrientation == UIInterfaceOrientationPortraitUpsideDown;
    }
    else
    {
        return   interfaceOrientation == UIInterfaceOrientationLandscapeRight
              || interfaceOrientation == UIInterfaceOrientationLandscapeLeft;
    }
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
}

-(BOOL)shouldAutorotate{
    // NOTE: Only for iOS6
    return YES;
}

-(UIInterfaceOrientationMask)supportedInterfaceOrientations {
    // NOTE: Only for iOS6
    return UIInterfaceOrientationMaskLandscape | UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
}

#pragma mark UIContentContainer

// Introduced in iOS8.0
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [self updateViewFramesWorkaround];
}

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
    if (_glfwWin.clientAPI == GLFW_NO_API)
    {
        return _glfwPlatformOpenWindowVulkan(width, height, wndconfig, fbconfig);
    }
    else
    {
        return _glfwPlatformOpenWindowOpenGL(width, height, wndconfig, fbconfig);
    }
}
