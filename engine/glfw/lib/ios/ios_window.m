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

static void LogGLError(GLint err)
{
#ifdef GL_ES_VERSION_2_0
    printf("gl error %d\n", err);
#else
    printf("gl error %d: %s\n", err, gluErrorString(err));
#endif
}

#define CHECK_GL_ERROR \
    { \
        GLint err = glGetError(); \
        if (err != 0) \
        { \
            LogGLError(err); \
            assert(0); \
        } \
    }\


/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView<UIKeyInput, UITextInputTraits> {

@private
    GLint backingWidth;
    GLint backingHeight;
    EAGLContext *context;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthRenderbuffer;
    CADisplayLink* displayLink;
    int countDown;
    int swapInterval;
    UIKeyboardType keyboardType;
}

- (void)swapBuffers;
- (void)newFrame;
- (void)setupView;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic) BOOL keyboardActive;
// TODO: Cooldown "timer" *hack* for backspace and enter release
#define TEXT_KEY_COOLDOWN (10)
@property (nonatomic) int textkeyActive;
@property (nonatomic) int autoCloseKeyboard;

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
    viewRenderbuffer = 0;
    viewFramebuffer = 0;
    depthRenderbuffer = 0;

  return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self.multipleTouchEnabled = YES;
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
        const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discards);

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

- (void) fill: (GLFWTouch*) glfwt withTouch: (UITouch*) t
{
    CGPoint touchLocation = [t locationInView:self];
    CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;
    int px = prevTouchLocation.x * scaleFactor;
    int py = prevTouchLocation.y * scaleFactor;

    glfwt->TapCount = t.tapCount;
    glfwt->Phase = t.phase;
    glfwt->X = x;
    glfwt->Y = y;
    glfwt->DX = x - px;
    glfwt->DY = y - py;
    // Store reference to for later for ordering comparison
    glfwt->Reference = t;
}

- (int) fillTouch: (UIEvent*) event
{
    NSSet *touches = [event allTouches];

    int touchCount = 0;

    // Keep order by first resuing previous elements
    for (int i = 0; i < _glfwInput.TouchCount; i++)
    {
        GLFWTouch* glfwt = &_glfwInput.Touch[i];

        // NOTE: Tried first with [touchces contains] but got spurious crashes
        int found = 0;
        for (UITouch *t in touches)
        {
            if (t == glfwt->Reference)
            {
                [self fill: glfwt withTouch: t];
                found = 1;
            }
        }

        if (!found)
            break;

        touchCount++;
    }

    for (UITouch *t in touches)
    {
         if (touchCount >= GLFW_MAX_TOUCH)
             break;

         int found = 0;
         // Check if already processed in the initial loop
         for (int i = 0; i < touchCount; i++)
         {
             GLFWTouch* glfwt = &_glfwInput.Touch[i];
             if (t == (UITouch*) glfwt->Reference)
             {
                 found = 1;
             }
         }
         if (found)
             continue;

         GLFWTouch* glfwt = &_glfwInput.Touch[touchCount];
         [self fill: glfwt withTouch: t];
         touchCount++;
    }
    return touchCount;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    _glfwInput.TouchCount = [self fillTouch: event];
    if( _glfwWin.touchCallback )
    {
        _glfwWin.touchCallback(_glfwInput.Touch, _glfwInput.TouchCount);
    }

    _glfwInput.MousePosX = _glfwInput.Touch[0].X;
    _glfwInput.MousePosY = _glfwInput.Touch[0].Y;

    if( _glfwWin.mousePosCallback )
    {
        _glfwWin.mousePosCallback( _glfwInput.MousePosX, _glfwInput.MousePosY );
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if (self.keyboardActive && self.autoCloseKeyboard) {
        // Implicitly hide keyboard
        _glfwShowKeyboard(0, 0, 0);
    }

    _glfwInput.TouchCount = [self fillTouch: event];
    if( _glfwWin.touchCallback )
    {
        _glfwWin.touchCallback(_glfwInput.Touch, _glfwInput.TouchCount);
    }

    _glfwInput.MousePosX = _glfwInput.Touch[0].X;
    _glfwInput.MousePosY = _glfwInput.Touch[0].Y;
    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    _glfwInput.TouchCount = [self fillTouch: event];
    if( _glfwWin.touchCallback )
    {
        _glfwWin.touchCallback(_glfwInput.Touch, _glfwInput.TouchCount);
    }

    _glfwInput.MousePosX = _glfwInput.Touch[0].X;
    _glfwInput.MousePosY = _glfwInput.Touch[0].Y;

    if (_glfwInput.TouchCount == 1)
    {
        // Send release for last finger
        _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
    }
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (BOOL)hasText
{
    return YES;
}

- (void)insertText:(NSString *)theText
{
    int length = [theText length];

    if (length == 1 && [theText characterAtIndex: 0] == 10) {
        _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
        return;
    }

    for(int i = 0;  i < length;  i++) {
        // Trick to "fool" glfw. Otherwise repeated characters will be filtered due to repeat
        _glfwInputChar( [theText characterAtIndex:i], GLFW_RELEASE );
        _glfwInputChar( [theText characterAtIndex:i], GLFW_PRESS );
    }
}

- (void)deleteBackward
{
    _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
    self.textkeyActive = TEXT_KEY_COOLDOWN;
}

- (UIKeyboardType) keyboardType
{
    return keyboardType;
}

- (void) setKeyboardType: (UIKeyboardType) type
{
    keyboardType = type;
}

- (UIReturnKeyType) returnKeyType
{
    return UIReturnKeyDefault;
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    _glfwInput.TouchCount = [self fillTouch: event];
    if( _glfwWin.touchCallback )
    {
        _glfwWin.touchCallback(_glfwInput.Touch, _glfwInput.TouchCount);
    }
    _glfwInput.MousePosX = _glfwInput.Touch[0].X;
    _glfwInput.MousePosY = _glfwInput.Touch[0].Y;
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
    _glfwWin.frameBuffer = viewFramebuffer;

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
    CHECK_GL_ERROR
    return YES;
}

- (void)destroyFramebuffer
{
    if (viewFramebuffer)
    {
        [context renderbufferStorage:GL_RENDERBUFFER fromDrawable: nil];
        CHECK_GL_ERROR

        glDeleteFramebuffers(1, &viewFramebuffer);
        CHECK_GL_ERROR
        viewFramebuffer = 0;
        glDeleteRenderbuffers(1, &viewRenderbuffer);
        CHECK_GL_ERROR
        viewRenderbuffer = 0;

        if(depthRenderbuffer)
        {
            glDeleteRenderbuffers(1, &depthRenderbuffer);
            CHECK_GL_ERROR
            depthRenderbuffer = 0;
        }
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

_GLFWwin g_Savewin;

@implementation AppDelegate

@synthesize window;

- (void)reinit:(UIApplication *)application
{

    // We used to recreate the view-controller and view here in order to trigger orientation logic.
    // The new method is to create a temporary view-contoller. See below.

    // NOTE: This code is *not* invoked from the built-in/traditional event-loop
    // hence *no* NSAutoreleasePool is setup. Without a pool here
    // the application would leak and specifically the ViewController
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Restore window data
    _glfwWin = g_Savewin;

    UIViewController *viewController = [[UIViewController alloc] init];
    [viewController setModalPresentationStyle:UIModalPresentationCurrentContext];
    viewController.view.frame = CGRectZero;
    [window.rootViewController presentModalViewController:viewController animated:NO];
    [window.rootViewController dismissModalViewControllerAnimated:YES];
    [viewController release];
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

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;
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
    // Save window as glfw clears the memory on close
    g_Savewin = _glfwWin;
}

int _glfwPlatformGetDefaultFramebuffer( )
{
    return _glfwWin.frameBuffer;
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

    EAGLView* view = (EAGLView*) _glfwWin.view;
    if (view.keyboardActive) {
        if (view.textkeyActive == 0) {
            _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
            _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
        } else {
            view.textkeyActive = view.textkeyActive - 1;
        }
    }
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

void _glfwShowKeyboard( int show, int type, int auto_close )
{
    EAGLView* view = (EAGLView*) _glfwWin.view;
    switch (type) {
        case GLFW_KEYBOARD_DEFAULT:
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        case GLFW_KEYBOARD_NUMBER_PAD:
            view.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case GLFW_KEYBOARD_EMAIL:
            view.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        default:
            view.keyboardType = UIKeyboardTypeDefault;
    }
    view.textkeyActive = -1;
    view.autoCloseKeyboard = auto_close;
    if (show) {
        view.keyboardActive = YES;
        [_glfwWin.view becomeFirstResponder];
    } else {
        view.keyboardActive = NO;
        [_glfwWin.view resignFirstResponder];
    }
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
