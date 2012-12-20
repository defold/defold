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

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

#include "internal.h"
#include "platform.h"

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

// OpenGL view

/*
 * Launching your Application in Landscape
 * https://developer.apple.com/library/ios/#technotes/tn2244/_index.html
 *
 * This is something we might want to add support for soon. The following is required:
 *   - Flip width/height when creating the window (in applicationDidFinishLaunching)
 *   - return (interfaceOrientation == UIInterfaceOrientationLandscapeRight);
 *     from shouldAutorotateToInterfaceOrientation
 *
 *   See TN2244 for more information
 */

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView {

@private
    GLint backingWidth;
    GLint backingHeight;
    EAGLContext *context;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthRenderbuffer;
    CADisplayLink* displayLink;
    int countDown;
    int swapInterval;
}

- (void)swapBuffers;
- (void)newFrame;
- (void)setupView;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end

@implementation EAGLView

@synthesize context;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id) init {
  self = [super init];
  if (self != nil) {
      [self setSwapInterval: 1];
  }
  return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _glfwWin.view = self;
    if ((self = [super initWithFrame:frame]))
    {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

        if (!context || ![EAGLContext setCurrentContext:context])
        {
            [self release];
            return nil;
        }

        displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(newFrame)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        displayLink.frameInterval = 1;

        [self setupView];
    }
    return self;
}

- (void)setupView
{
}

- (void)swapBuffers
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    while (countDown > 0)
    {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, TRUE);
    }
    [pool release];

    countDown = swapInterval;

    // NOTE: We poll events above and the application might be iconfied
    // At least when running in frame-rates < 60
    if (!_glfwWin.iconified)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

- (void)newFrame
{
    countDown--;
}

- (void) setSwapInterval: (int) interval
{
    if (interval < 1)
    {
        interval = 1;
    }
    swapInterval = interval;
    countDown = swapInterval;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    CGFloat scaleFactor = self.contentScaleFactor;
    _glfwInput.MousePosX = touchLocation.x * scaleFactor;
    _glfwInput.MousePosY = touchLocation.y * scaleFactor;

    if( _glfwWin.mousePosCallback )
    {
        _glfwWin.mousePosCallback( _glfwInput.MousePosX, _glfwInput.MousePosY );
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    CGFloat scaleFactor = self.contentScaleFactor;
    _glfwInput.MousePosX = touchLocation.x * scaleFactor;
    _glfwInput.MousePosY = touchLocation.y * scaleFactor;

    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    CGFloat scaleFactor = self.contentScaleFactor;
    _glfwInput.MousePosX = touchLocation.x * scaleFactor;
    _glfwInput.MousePosY = touchLocation.y * scaleFactor;

    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
}

- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self swapBuffers];
}

- (BOOL)createFramebuffer
{
    glGenFramebuffers(1, &viewFramebuffer);
    glGenRenderbuffers(1, &viewRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    _glfwWin.width = backingWidth;
    _glfwWin.height = backingHeight;

    if (_glfwWin.windowSizeCallback)
    {
        _glfwWin.windowSizeCallback( backingWidth, backingHeight );
    }

    // Setup depth buffers
    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }

    return YES;
}

- (void)destroyFramebuffer
{
    glDeleteFramebuffers(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffers(1, &viewRenderbuffer);
    viewRenderbuffer = 0;

    if(depthRenderbuffer)
    {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}

- (void)dealloc
{
    if (displayLink != 0)
    {
        [displayLink release];
    }
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [EAGLContext setCurrentContext:nil];
    [context release];
    [super dealloc];
}

@end


//========================================================================
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================


// View controller

@interface ViewController : UIViewController<UIAccelerometerDelegate>
{
    EAGLView *glView;
}


@property (nonatomic, retain) IBOutlet EAGLView *glView;

@end

@implementation ViewController

@synthesize glView;

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizesSubviews = YES;
    CGRect bounds = self.view.bounds;

    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    glView = [[[EAGLView alloc] initWithFrame: bounds] autorelease ];
    glView.contentScaleFactor = scaleFactor;
    glView.layer.contentsScale = scaleFactor;
    [ [self view] addSubview: glView ];
    [[UIAccelerometer sharedAccelerometer] setUpdateInterval:1.0/60.0];
    [[UIAccelerometer sharedAccelerometer] setDelegate:self];

    float version = [[UIDevice currentDevice].systemVersion floatValue];

    if (version < 6)
    {
        // NOTE: This is only required for older versions of iOS
        // In iOS 6 new method was introduced for rotation logc, see AppDelegate
        UIInterfaceOrientation orientation = _glfwWin.portrait ? UIInterfaceOrientationPortrait : UIInterfaceOrientationLandscapeRight;
        [[UIApplication sharedApplication] setStatusBarOrientation: orientation animated: NO];
    }
}

- (void)viewDidAppear:(BOOL)animated
{
    // NOTE: We rely on an active OpenGL-context as we have no concept of Begin/End rendering
    // As we replace view-controller and view when re-opening the "window" we must ensure that we always
    // have an active context (context is set to nil when view is deallocated)
    [EAGLContext setCurrentContext: glView.context];
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

-(NSInteger)supportedInterfaceOrientations {
    // NOTE: Only for iOS6
    if (_glfwWin.portrait)
    {
        return UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
    }
    else
    {
        return UIInterfaceOrientationMaskLandscape;
    }
}

- (void)accelerometer:(UIAccelerometer *)accelerometer didAccelerate:(UIAcceleration *)acceleration
{
    _glfwInput.AccX = acceleration.x;
    _glfwInput.AccY = acceleration.y;
    _glfwInput.AccZ = acceleration.z;
}
@end

// Application delegate

@interface AppDelegate : NSObject <UIApplicationDelegate>
{
    UIWindow *window;
}

- (void)reinit:(UIApplication *)application;

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end


@implementation AppDelegate

@synthesize window;

- (void)reinit:(UIApplication *)application
{
    // Recreation of view-controller and view. When a view-controller and view is associated with an window
    // the orientation logic is triggered, supportedInterfaceOrientations, etc.
    // Ideally we should not re-create view-controller and view but
    // triggering that logic was difficult/impossible. We could use a private method in UIScreen to force orientation
    // but that method is private and not documented. This is simple and works.

    // NOTE: This code is *not* invoked from the built-in/traditional event-loop
    // hence *no* NSAutoreleasePool is setup. Without a pool here
    // the application would leak and specifcally the ViewController
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    window.rootViewController = nil;
    window.rootViewController = [[[ViewController alloc] init] autorelease];

    [pool release];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
    // NOTE: On iPhone4 the "resolution" is 480x320 and not 960x640
    // Points vs pixels (and scale factors). I'm not sure that this correct though
    // and that we really get the correct and highest physical resolution in pixels.
    CGRect bounds = [UIScreen mainScreen].bounds;

    window = [[UIWindow alloc] initWithFrame:bounds];
    window.rootViewController = [[[ViewController alloc] init] autorelease];
    [window makeKeyAndVisible];
    [application setIdleTimerDisabled: YES];

    // We can't hijack the event loop here. We post-pone it to ensure that the application is
    // completely initialized
    [NSTimer scheduledTimerWithTimeInterval:0.001 target:self selector:@selector(hijackEventLoop) userInfo:nil repeats:NO];
}

- (void)hijackEventLoop
{
    // Long jump back to start and run our own event-loop/game-loop
    longjmp(_glfwWin.bailEventLoopBuf,1);
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // We should pause the update loop when this message is sent
    _glfwWin.iconified = GL_TRUE;

    // According to Apple glFinish() should be called here
    glFinish();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    _glfwWin.iconified = GL_FALSE;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

- (void)dealloc
{
    [window release];
    [super dealloc];
}

@end


int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    /*
     * This is somewhat of a hack. We can't recreate the application here.
     * Instead we reinit the app and return and keep application and windows as is
     * We should either move application creation to glfwInit or skip glfw altogether.
     */
    UIApplication* app = [UIApplication sharedApplication];
    if (app)
    {
        AppDelegate* delegate = app.delegate;
        [delegate reinit: app];
        return GL_TRUE;
    }

    _glfwWin.pixelFormat = nil;
    _glfwWin.window = nil;
    _glfwWin.context = nil;
    _glfwWin.delegate = nil;
    _glfwWin.view = nil;

    /*
     * NOTE:
     * We ignore the following
     * wndconfig->*
     * fbconfig->*
     */

    if (!setjmp(_glfwWin.bailEventLoopBuf) )
    {
        char* argv[] = { "dummy" };
        int retVal = UIApplicationMain(1, argv, nil, @"AppDelegate");
        (void) retVal;
    }
    else
    {
    }

    return GL_TRUE;
}

//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
}

//========================================================================
// Iconify the window
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
}

//========================================================================
// Restore (un-iconify) the window
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}

//========================================================================
// Swap buffers
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    [ _glfwWin.view swapBuffers ];
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    [ _glfwWin.view setSwapInterval: interval ];
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}

//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    SInt32 result;
    do {
        result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, TRUE);
    } while (result == kCFRunLoopRunHandledSource);
    [pool release];
}

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    *x = _glfwInput.AccX;
    *y = _glfwInput.AccY;
    *z = _glfwInput.AccZ;
    return 1;
}
