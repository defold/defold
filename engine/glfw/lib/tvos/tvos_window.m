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
#import <GameController/GameController.h>

#include "internal.h"
#include "platform.h"


enum StartupPhase
{
    INITIAL,
    INIT1,
    INIT2,
    COMPLETE,
};

enum StartupPhase g_StartupPhase = INITIAL;
void* g_ReservedStack = 0;
id g_rootViewController = 0;
int g_SwapCount = 0;

static int g_IsReboot = 0;
/*
Notes about the crazy startup
In order to have a classic event-loop we must "bail" the built-in event dispatch loop
using setjmp/longjmp. Moreover, data must be loaded before applicationDidFinishLaunching
is completed as the launch image is removed moments after applicationDidFinishLaunching finish.
It's also imperative that applicationDidFinishLaunching completes entirely. If not, the launch image
isn't removed as the startup sequence isn't completed properly. Something that complicates this matter
is that it isn't allowed to longjmp in a setjmp as the stack is squashed. By allocating a lot
of stack *before* UIApplicationMain is invoked we can be quite confident that stack used by UIApplicationMain
and descendants is kept intact. This is a crazy hack but we don't have much of a choice. Only
alternative is to modify glfw to have a structure similar to Cocoa Touch.

Additionally we postpone startup sequence until we have swapped gl-buffers twice in
order to avoid black screen between launch image and game content.
*/


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


#define MAX_APP_DELEGATES (32)
id<UIApplicationDelegate> g_AppDelegates[MAX_APP_DELEGATES];
int g_AppDelegatesCount = 0;
id<UIApplicationDelegate> g_ApplicationDelegate = 0;

@interface AppDelegateProxy : NSObject <UIApplicationDelegate>

@end

@implementation AppDelegateProxy

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    BOOL invoked = NO;
    if ([g_ApplicationDelegate respondsToSelector: [anInvocation selector]]) {
        [anInvocation invokeWithTarget: g_ApplicationDelegate];
        invoked = YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: [anInvocation selector]]) {
            [anInvocation invokeWithTarget: g_AppDelegates[i]];
            invoked = YES;
        }
    }

    if (!invoked) {
        [super forwardInvocation:anInvocation];
    }
}

- (BOOL)respondsToSelector:(SEL)aSelector {

    if ([g_ApplicationDelegate respondsToSelector: aSelector]) {
        return YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
            return YES;
        }
    }

    return [super respondsToSelector: aSelector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature* signature = [super methodSignatureForSelector:aSelector];

    if (!signature)
    {
        for (int i = 0; i < g_AppDelegatesCount; ++i) {
            if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
                return [g_AppDelegates[i] methodSignatureForSelector:aSelector];
            }
        }
    }
    return signature;
}

@end

GLFWAPI void glfwRegisterUIApplicationDelegate(void* delegate)
{
    if (g_AppDelegatesCount >= MAX_APP_DELEGATES) {
        printf("Max UIApplicationDelegates reached (%d)", MAX_APP_DELEGATES);
    } else {
        g_AppDelegates[g_AppDelegatesCount++] = (id<UIApplicationDelegate>) delegate;
    }
}

GLFWAPI void glfwUnregisterUIApplicationDelegate(void* delegate)
{
    assert(g_AppDelegatesCount > 0);
    for (int i = 0; i < g_AppDelegatesCount; ++i)
    {
        if (g_AppDelegates[i] == delegate)
        {
            g_AppDelegates[i] = g_AppDelegates[g_AppDelegatesCount - 1];
            g_AppDelegatesCount--;
            return;
        }
    }
    assert(false && "app delegate not found");
}

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView<UITextFieldDelegate> {

@private
    GLint backingWidth;
    GLint backingHeight;
    EAGLContext *context;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthStencilRenderbuffer;
    CADisplayLink* displayLink;
    int countDown;
    int swapInterval;
}

- (void)swapBuffers;
- (void)newFrame;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, retain) UITextField *triggerField;
@property (nonatomic, retain) UILabel* label;
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
    depthStencilRenderbuffer = 0;

    _glfwInput.MouseEmulationTouch = 0;
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

        displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(newFrame)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        displayLink.frameInterval = 1;

        self.triggerField = [[UITextField alloc] init];
        self.triggerField.hidden = YES;
        self.triggerField.clearsOnBeginEditing = YES;
        self.triggerField.delegate = self;

        self.label = [[UILabel alloc] initWithFrame:CGRectMake(20, 20, 1920, 100)];
        self.label.font = [self.label.font fontWithSize:48];
        self.triggerField.inputAccessoryView = self.label;

        [self.triggerField addTarget:self action:@selector(textFieldDidEndEditing:) forControlEvents:UIControlEventEditingDidEnd];
        [self addSubview:self.triggerField];
    }
    return self;
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string {
    if ([string UTF8String][0] == 0) {
        _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
    } else {
        int length = [string length];
        if (length == 1 && [string characterAtIndex: 0] == 10) {
            return NO;
        }
        for(int i = 0;  i < length;  i++) {
            // Trick to "fool" glfw. Otherwise repeated characters will be filtered due to repeat
            _glfwInputChar( [string characterAtIndex:i], GLFW_RELEASE );
            _glfwInputChar( [string characterAtIndex:i], GLFW_PRESS );
        }
    }
    return YES;
}

-(void)textFieldDidEndEditing :(UITextField *)theTextField{
    if (self.keyboardActive && self.autoCloseKeyboard) {
        _glfwShowKeyboard(0, 0, 0);
    }
}

- (void)swapBuffers
{
    if (g_StartupPhase == COMPLETE) {
        // Do not poll event before startup sequence is completed
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        while (countDown > 0)
        {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, TRUE);
        }
        [pool release];

        countDown = swapInterval;
    }

    g_SwapCount++;

    // NOTE: We poll events above and the application might be iconfied
    // At least when running in frame-rates < 60
    if (!_glfwWin.iconified && g_StartupPhase == COMPLETE)
    {
        const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discards);

        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context presentRenderbuffer:GL_RENDERBUFFER];

        [g_rootViewController pollControllers];
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

    // Setup packed depth and stencil buffers
    glGenRenderbuffers(1, &depthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);

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
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        CHECK_GL_ERROR
        [context renderbufferStorage:GL_RENDERBUFFER fromDrawable: nil];
        CHECK_GL_ERROR

        glDeleteFramebuffers(1, &viewFramebuffer);
        CHECK_GL_ERROR
        viewFramebuffer = 0;
        glDeleteRenderbuffers(1, &viewRenderbuffer);
        CHECK_GL_ERROR
        viewRenderbuffer = 0;

        if(depthStencilRenderbuffer)
        {
            glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
            CHECK_GL_ERROR
            depthStencilRenderbuffer = 0;
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

@interface ViewController : UIViewController<UIContentContainer>
{
    EAGLView *glView;
}

- (EAGLContext *)initialiseGlContext;
- (void)createGlView;
- (void)pollControllers;


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

    [self createGlView];
}

-(void)pollControllers {
    if (self.glView.keyboardActive) {
        return;
    }

    for(int i=0; i<GLFW_JOYSTICK_LAST; ++i) {
        if (tvosJoystick[i].present != 0) {
            if ([GCController controllers][i].microGamepad == nil) {
                if ([GCController controllers][i].extendedGamepad == nil) {
                    GCGamepad* profile =[GCController controllers][i].gamepad;
                    tvosJoystick[i].axes[0] = profile.dpad.xAxis.value;
                    tvosJoystick[i].axes[1] = profile.dpad.yAxis.value;

                    tvosJoystick[i].buttons[0] = profile.buttonA.isPressed;
                    tvosJoystick[i].buttons[1] = profile.buttonB.isPressed;
                    tvosJoystick[i].buttons[2] = profile.buttonX.isPressed;
                    tvosJoystick[i].buttons[3] = profile.buttonY.isPressed;
                    tvosJoystick[i].buttons[4] = profile.leftShoulder.isPressed;
                    tvosJoystick[i].buttons[5] = profile.rightShoulder.isPressed;

                } else {
                    GCExtendedGamepad* profile =[GCController controllers][i].extendedGamepad;

                    tvosJoystick[i].axes[0] = profile.dpad.xAxis.value;
                    tvosJoystick[i].axes[1] = profile.dpad.yAxis.value;
                    tvosJoystick[i].axes[2] = profile.leftThumbstick.xAxis.value;
                    tvosJoystick[i].axes[3] = profile.leftThumbstick.yAxis.value;
                    tvosJoystick[i].axes[4] = profile.rightThumbstick.xAxis.value;
                    tvosJoystick[i].axes[5] = profile.rightThumbstick.yAxis.value;

                    tvosJoystick[i].buttons[0] = profile.buttonA.isPressed;
                    tvosJoystick[i].buttons[1] = profile.buttonB.isPressed;
                    tvosJoystick[i].buttons[2] = profile.buttonX.isPressed;
                    tvosJoystick[i].buttons[3] = profile.buttonY.isPressed;
                    tvosJoystick[i].buttons[4] = profile.leftShoulder.isPressed;
                    tvosJoystick[i].buttons[5] = profile.rightShoulder.isPressed;
                    tvosJoystick[i].buttons[6] = profile.leftTrigger.isPressed;
                    tvosJoystick[i].buttons[7] = profile.rightTrigger.isPressed;
                }
            }
            else {
                GCMicroGamepad* profile =[GCController controllers][i].microGamepad;

                tvosJoystick[i].axes[0] = profile.dpad.xAxis.value;
                tvosJoystick[i].axes[1] = profile.dpad.yAxis.value;
                tvosJoystick[i].buttons[0] = profile.buttonA.isPressed;
                tvosJoystick[i].buttons[1] = profile.buttonX.isPressed;
            }
        }
    }
}

- (void)controllerStateChanged {
    // Look for new controllers and handle them
    for(unsigned long i=0,l=[[GCController controllers] count]; i<l; ++i) {
        if ([GCController controllers][i]) {

            if (tvosJoystick[i].present == 0) {
                tvosJoystick[i].present = 1;
                [GCController controllers][i].playerIndex = i;

                if ([GCController controllers][i].microGamepad == nil) {
                    if ([GCController controllers][i].extendedGamepad == nil) {
                        // gamepad
                        tvosJoystick[i].numAxes = 2;
                        tvosJoystick[i].numButtons = 6;
                    } else {
                        // extendedGamepad
                        tvosJoystick[i].numAxes = 6;
                        tvosJoystick[i].numButtons = 8;
                    }
                }
                else {
                    // microGamepad
                    tvosJoystick[i].numAxes = 2;
                    tvosJoystick[i].numButtons = 2;
                    GCMicroGamepad *profile = [GCController controllers][i].microGamepad;
                    profile.valueChangedHandler = ^ (GCMicroGamepad *gamepad, GCControllerElement *element) {
                        GCController* ctrl = [GCController controllers][i];
                        if (ctrl.motion) {
                            tvosJoystick[i].gravity[0] = ctrl.motion.gravity.x;
                            tvosJoystick[i].gravity[1] = ctrl.motion.gravity.y;
                            tvosJoystick[i].gravity[2] = ctrl.motion.gravity.z;
                            tvosJoystick[i].acceleration[0] = ctrl.motion.userAcceleration.x;
                            tvosJoystick[i].acceleration[1] = ctrl.motion.userAcceleration.y;
                            tvosJoystick[i].acceleration[2] = ctrl.motion.userAcceleration.z;
                        }
                    };
                }

                tvosJoystick[i].axes = (float*)malloc(sizeof(float) * tvosJoystick[i].numAxes);
                tvosJoystick[i].buttons = (unsigned char*)malloc(sizeof(char) * tvosJoystick[i].numButtons);
            }
        }
    }

    // Check for removed controllers and handle
    for(int i=0; i<GLFW_JOYSTICK_LAST; ++i) {

        if (tvosJoystick[i].present == 1) {
            bool found = false;
            for(unsigned long j=0,l=[[GCController controllers] count]; !found && j<l; ++j) {
                found = [GCController controllers][j].playerIndex == i;
            }
            if (!found) {
                tvosJoystick[i].present = 0;
                free(tvosJoystick[i].axes);
                free(tvosJoystick[i].buttons);
                memset(&tvosJoystick[i], 0, sizeof(struct tvosJoystickData));
            }
        }
    }
}

- (void)createGlView
{
    EAGLContext* glContext = nil;
    if (glView) {
        // We must recycle the GL context, since the engine will be performing operations
        // (e.g. creating shaders and textures) that depend upon it.
        glContext = glView.context;
        [glView removeFromSuperview];
    }

    if (!glContext) {
        glContext = [self initialiseGlContext];
    }

    CGRect bounds = self.view.bounds;
    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    glView = [[[EAGLView alloc] initWithFrame: bounds] autorelease];
    glView.context = glContext;
    glView.contentScaleFactor = scaleFactor;
    glView.layer.contentsScale = scaleFactor;
    [[self view] addSubview:glView];

    for(int i=0; i<GLFW_JOYSTICK_LAST; ++i) {
        memset(&tvosJoystick[i], 0, sizeof(struct tvosJoystickData));
    }

    [glView createFramebuffer];
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

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerStateChanged) name:GCControllerDidConnectNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerStateChanged) name:GCControllerDidDisconnectNotification object:nil];

    [GCController startWirelessControllerDiscoveryWithCompletionHandler:^{
        [self controllerStateChanged];
    }];

    if (g_StartupPhase == INIT2) {
        longjmp(_glfwWin.bailEventLoopBuf, 1);
    }
}

- (void)viewDidUnload
{
    [super viewDidUnload];
}

#pragma mark UIContentContainer


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
    g_IsReboot = 1;

    // Restore window data
    _glfwWin = g_Savewin;

    // To avoid a race, since _glfwPlatformOpenWindow does not block,
    // update the glfw's cached screen dimensions ahead of time.
    BOOL flipScreen = NO;
    if (_glfwWin.portrait) {
        flipScreen = _glfwWin.width > _glfwWin.height;
    } else {
        flipScreen = _glfwWin.width < _glfwWin.height;
    }
    if (flipScreen) {
        float tmp = _glfwWin.width;
        _glfwWin.width = _glfwWin.height;
        _glfwWin.height = tmp;
    }

    // We then rebuild the GL view back within the application's event loop.
    dispatch_async(dispatch_get_main_queue(), ^{
        ViewController *controller = (ViewController *)window.rootViewController;
        [controller createGlView];
    });
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    // NOTE: On iPhone4 the "resolution" is 480x320 and not 960x640
    // Points vs pixels (and scale factors). I'm not sure that this correct though
    // and that we really get the correct and highest physical resolution in pixels.
    CGRect bounds = [UIScreen mainScreen].bounds;

    window = [[UIWindow alloc] initWithFrame:bounds];
    window.rootViewController = [[[ViewController alloc] init] autorelease];
    g_rootViewController = window.rootViewController;

    [window makeKeyAndVisible];

    UIApplication* app = [UIApplication sharedApplication];
    AppDelegateProxy* proxy = [[AppDelegateProxy alloc] init];
    g_ApplicationDelegate = [app.delegate retain];
    app.delegate = proxy;

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(applicationDidFinishLaunching:)]) {
            [g_AppDelegates[i] applicationDidFinishLaunching: application];
        }
    }

    if (!setjmp(_glfwWin.finishInitBuf))
    {
        g_StartupPhase = INIT1;
        longjmp(_glfwWin.bailEventLoopBuf, 1);
    }
    else
    {
        g_StartupPhase = INIT2;
    }

    BOOL handled = NO;

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(application:didFinishLaunchingWithOptions:)]) {
            if ([g_AppDelegates[i] application:application didFinishLaunchingWithOptions:launchOptions]) {
                handled = YES;
            }
        }
    }
    return handled;
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
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(0);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    _glfwWin.iconified = GL_FALSE;
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(1);
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
        [g_ApplicationDelegate reinit: app];
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

    const int stack_size = 1 << 18;
    // Store stack pointer in a global variable.
    // Otherwise the allocated stack might be removed by the optimizer
    g_ReservedStack = alloca(stack_size);
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
// Set the text input title
//========================================================================

void _glfwPlatformSetTextInputTitle( const char *title )
{
    EAGLView* view = (EAGLView*) _glfwWin.view;
    view.label.text = [NSString stringWithUTF8String:title];
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
    if (g_StartupPhase == INIT1 && g_SwapCount > 1) {
        if (!setjmp(_glfwWin.bailEventLoopBuf))
        {
            longjmp(_glfwWin.finishInitBuf, 1);
        }
        else
        {
            g_StartupPhase = COMPLETE;
        }
        return;
    }

    if (g_StartupPhase != COMPLETE) {
        return;
    }

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
            view.triggerField.keyboardType = UIKeyboardTypeDefault;
            break;
        case GLFW_KEYBOARD_NUMBER_PAD:
            view.triggerField.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case GLFW_KEYBOARD_EMAIL:
            view.triggerField.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        default:
            view.triggerField.keyboardType = UIKeyboardTypeDefault;
    }
    view.textkeyActive = -1;
    view.autoCloseKeyboard = auto_close;
    if (show) {
        view.keyboardActive = YES;
        [view.triggerField becomeFirstResponder];
    } else {
        view.keyboardActive = NO;
        [view.triggerField resignFirstResponder];
    }
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    return 0;
}
