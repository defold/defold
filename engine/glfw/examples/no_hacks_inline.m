
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>



#define HID_MAX_GAMEPAD_COUNT (16)

struct tvosJoystickData {
    char present;
    int numAxes;
    int numButtons;
    float* axes;
    unsigned char* buttons;
};

@interface Gamepad : NSObject {

    struct tvosJoystickData joy[HID_MAX_GAMEPAD_COUNT];
}
@end

@interface Gamepad ()

-(void)pollControllers;

@end


@interface EAGLView : UIView {

@private
    EAGLContext *context;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLint backingWidth, backingHeight;
    GLuint depthStencilRenderbuffer;
    CADisplayLink* displayLink;
    Gamepad* gamepads;
}

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, retain) Gamepad *gamepads;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
{
    UIWindow *window;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end



@interface ViewController : UIViewController<UIContentContainer>
{
    EAGLView *glView;
@public
    Gamepad* gamepads;
}

- (EAGLContext *)initialiseGlContext;
- (void)createGlView;

@property (nonatomic, retain) IBOutlet EAGLView *glView;

@end







@implementation Gamepad

- (id) init {
    self = [super init];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerStateChanged) name:GCControllerDidConnectNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerStateChanged) name:GCControllerDidDisconnectNotification object:nil];
    
    [GCController startWirelessControllerDiscoveryWithCompletionHandler:^{
        [self controllerStateChanged];
    }];

    return self;
}

- (void)controllerStateChanged {
    
    NSLog(@"controllerStateChanged\n");
    
    // TODO: Handle disconnects!
    for(unsigned long i=0,l=[[GCController controllers] count]; i<l; ++i) {
        if ([GCController controllers][i]) {
            if (joy[i].present == 0) {
                joy[i].present = 1;
                
                if ([GCController controllers][i].microGamepad == nil) {
                    if ([GCController controllers][i].extendedGamepad == nil) {
                        joy[i].numAxes = 6;
                        joy[i].numButtons = 8;
                    } else {
                        joy[i].numAxes = 2;
                        joy[i].numButtons = 6;
                    }
                }
                else {
                    joy[i].numAxes = 2;
                    joy[i].numButtons = 2;
                }
                joy[i].axes = (float*)malloc(sizeof(float) * joy[i].numAxes);
                joy[i].buttons = (unsigned char*)malloc(sizeof(char) * joy[i].numButtons);
            }
        }
    }
    
}

-(void)pollControllers {
    for(int i=0; i<HID_MAX_GAMEPAD_COUNT; ++i) {
        if (joy[i].present != 0) {
            
            if ([GCController controllers][i].microGamepad == nil) {
                if ([GCController controllers][i].extendedGamepad == nil) {
                    GCGamepad* profile =[GCController controllers][i].gamepad;
                    joy[i].axes[0] = profile.dpad.xAxis.value;
                    joy[i].axes[1] = profile.dpad.yAxis.value;
                    joy[i].buttons[0] = profile.buttonA.isPressed;
                    joy[i].buttons[1] = profile.buttonB.isPressed;
                    joy[i].buttons[2] = profile.buttonX.isPressed;
                    joy[i].buttons[3] = profile.buttonY.isPressed;
                    joy[i].buttons[4] = profile.leftShoulder.isPressed;
                    joy[i].buttons[5] = profile.rightShoulder.isPressed;
                    
                } else {
                    GCExtendedGamepad* profile =[GCController controllers][i].extendedGamepad;
                    
                    joy[i].axes[0] = profile.dpad.xAxis.value;
                    joy[i].axes[1] = profile.dpad.yAxis.value;
                    joy[i].axes[2] = profile.leftThumbstick.xAxis.value;
                    joy[i].axes[3] = profile.leftThumbstick.yAxis.value;
                    joy[i].axes[4] = profile.rightThumbstick.xAxis.value;
                    joy[i].axes[5] = profile.rightThumbstick.yAxis.value;
                    
                    joy[i].buttons[0] = profile.buttonA.isPressed;
                    joy[i].buttons[1] = profile.buttonB.isPressed;
                    joy[i].buttons[2] = profile.buttonX.isPressed;
                    joy[i].buttons[3] = profile.buttonY.isPressed;
                    joy[i].buttons[4] = profile.leftShoulder.isPressed;
                    joy[i].buttons[5] = profile.rightShoulder.isPressed;
                    joy[i].buttons[6] = profile.leftTrigger.isPressed;
                    joy[i].buttons[7] = profile.rightTrigger.isPressed;
                }
            }
            else {
                GCMicroGamepad* profile =[GCController controllers][i].microGamepad;
                joy[i].axes[0] = profile.dpad.xAxis.value;
                joy[i].axes[1] = profile.dpad.yAxis.value;
                joy[i].buttons[0] = profile.buttonA.isPressed;
                joy[i].buttons[1] = profile.buttonX.isPressed;

                NSLog(@"GCMicroGamepad %f, %f\n", joy[i].axes[0], joy[i].axes[1]);
            }
        }
    }
}

@end







@implementation EAGLView

@synthesize context;
@synthesize gamepads;



+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id) init {
    
    self = [super init];

    viewRenderbuffer = 0;
    viewFramebuffer = 0;
    depthStencilRenderbuffer = 0;
    gamepads = NULL;

    return self;
}

- (void)dealloc
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [EAGLContext setCurrentContext:nil];
    [context release];
    [super dealloc];
}

- (void)newFrame
{
    [self swapBuffers];
}

- (void)testKeyboard
{
    NSLog(@"grisapan!112\n");
    CGRect size = CGRectMake(20.0f, 100.0f, 280.0f, 31.0f);
    UITextField* triggerField = [[UITextField alloc] initWithFrame:size];
    triggerField.hidden = NO;
    [self addSubview:triggerField];

    [triggerField performSelector:@selector(becomeFirstResponder) withObject:nil afterDelay:2];
}

- (id)initWithFrame:(CGRect)frame
{
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    if ((self = [super initWithFrame:frame]))
    {
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:NO],
                                        kEAGLDrawablePropertyRetainedBacking,
                                        kEAGLColorFormatRGBA8,
                                        kEAGLDrawablePropertyColorFormat, nil];

        displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(newFrame)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        
        [self testKeyboard];
    }
    return self;
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
    return YES;
}

- (void)destroyFramebuffer
{
    if (viewFramebuffer)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context renderbufferStorage:GL_RENDERBUFFER fromDrawable: nil];
        glDeleteFramebuffers(1, &viewFramebuffer);
        viewFramebuffer = 0;
        glDeleteRenderbuffers(1, &viewRenderbuffer);
        viewRenderbuffer = 0;
        
        if(depthStencilRenderbuffer)
        {
            glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
            depthStencilRenderbuffer = 0;
        }
    }
}

- (void)swapBuffers
{
    static float r = 0, g = 0, b = 0;
    glClearColor(sinf(r) * 0.5f + 0.5f, sinf(g) * 0.5f + 0.5f, sinf(b) * 0.5f + 0.5f, 1);
    r += 0.05f;
    g += 0.014f;
    b += 0.024f;

    glClear(GL_COLOR_BUFFER_BIT);
    
    const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discards);
    
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];

    [gamepads pollControllers];
}

- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self swapBuffers];
}

@end



@implementation ViewController

@synthesize glView;


- (EAGLContext *)initialiseGlContext
{
    EAGLContext *context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if (!context || ![EAGLContext setCurrentContext:context]) {
        return nil;
    }
    
    return context;
}

- (void)createGlView
{
    EAGLContext* glContext = nil;
    if (glView) {
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
    
    gamepads = [[[Gamepad alloc] init] autorelease];
    glView.gamepads = gamepads;
    [glView createFramebuffer];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self createGlView];
}

- (void)viewDidAppear:(BOOL)animated
{
    [EAGLContext setCurrentContext: glView.context];
    [super viewDidAppear: animated];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
}

- (void)dealloc
{
    [glView release];
    [super dealloc];
}

@end









@interface AppDelegate ()

@end

@implementation AppDelegate

@synthesize window;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    CGRect bounds = [UIScreen mainScreen].bounds;
    window = [[UIWindow alloc] initWithFrame:bounds];
    window.rootViewController = [[[ViewController alloc] init] autorelease];
    [window makeKeyAndVisible];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    glFinish();
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
}

- (void)applicationWillTerminate:(UIApplication *)application {
}

- (void)dealloc
{
    [window release];
    [super dealloc];
}

@end


int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
