#import "ViewController.h"
#include "../../../include/GL/glfw.h"
#include "internal.h"

extern int g_IsReboot;

EAGLContext* g_glContext = nil;
EAGLContext* g_glAuxContext = nil;

@implementation ViewController

@synthesize glView;

- (void)dealloc
{
    [glView release];
    [super dealloc];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizesSubviews = YES;

    [self createGlView];

    _glfwWin.viewController = self;

    float version = [[UIDevice currentDevice].systemVersion floatValue];

    if (version < 6)
    {
        // NOTE: This is only required for older versions of iOS
        // In iOS 6 new method was introduced for rotation logc, see AppDelegate
        UIInterfaceOrientation orientation = UIInterfaceOrientationPortrait;
        [[UIApplication sharedApplication] setStatusBarOrientation: orientation animated: NO];
    }
}

- (EAGLContext *)initialiseGlAuxContext:(EAGLContext *)parentContext
{
    EAGLContext *context = [[EAGLContext alloc] initWithAPI:[parentContext API] sharegroup: [parentContext sharegroup]];

    if (!context)
    {
        return nil;
    }

    return context;
}

- (void)createGlView
{
    EAGLContext* glContext = nil;
    EAGLContext* glAuxContext = nil;
    if (glView) {
        // We must recycle the GL context, since the engine will be performing operations
        // (e.g. creating shaders and textures) that depend upon it.
        glContext = glView.context;
        glAuxContext = glView.auxContext;
        [glView removeFromSuperview];
    }

    if (!glContext) {
        glContext = [self initialiseGlContext];
        glAuxContext = [self initialiseGlAuxContext: glContext];
    }
    g_glContext = glContext;
    g_glAuxContext = glAuxContext;
    // _glfwWin.context = glContext;
    // _glfwWin.aux_context = glAuxContext;

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

    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    glView = [[[EAGLView alloc] initWithFrame: bounds] autorelease];
    glView.context = glContext;
    glView.auxContext = glAuxContext;
    glView.contentScaleFactor = scaleFactor;
    glView.layer.contentsScale = scaleFactor;
    [[self view] insertSubview:glView atIndex:0];

    [glView createFramebuffer];
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
        glView.frame = parent_bounds;
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

- (EAGLContext *)initialiseGlContext
{
    EAGLContext *context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (!context || ![EAGLContext setCurrentContext:context])
    {
        return nil;
    }

    return context;
}

- (void)viewDidAppear:(BOOL)animated
{
    // NOTE: We rely on an active OpenGL-context as we have no concept of Begin/End rendering
    // As we replace view-controller and view when re-opening the "window" we must ensure that we always
    // have an active context (context is set to nil when view is deallocated)
    [EAGLContext setCurrentContext: glView.context];

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

