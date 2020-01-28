#import "AppDelegate.h"
#import "AppDelegateProxy.h"
#import "ViewController.h"

#include "internal.h"

int g_IsReboot = 0;

@implementation AppDelegate

// Yes, these should be cleaned up up as well and not be stored in global variables! /MAWE

UIWindow*           g_ApplicationWindow = 0;

_GLFWwin            g_Savewin;

EngineCreate        g_EngineCreateFn = 0;
EngineDestroy       g_EngineDestroyFn = 0;
EngineUpdate        g_EngineUpdateFn = 0;
EngineGetResult     g_EngineResultFn = 0;
void*               g_EngineContext = 0;

int                 g_WasRebooted = 0;
int                 g_Argc = 0;
char**              g_Argv = 0;

@synthesize window;

- (void)forceDeviceOrientation;
{
    // Running iOS8, if we don't force a change in the device orientation then
    // the framebuffer will not be created with the correct orientation.
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation desired = UIDeviceOrientationLandscapeRight;
    if (_glfwWin.portrait) {
        desired = UIDeviceOrientationPortrait;
    } else if (UIDeviceOrientationLandscapeLeft == device.orientation) {
        desired = UIDeviceOrientationLandscapeLeft;
    }
    [device setValue: [NSNumber numberWithInteger: desired] forKey:@"orientation"];
}

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

    [self forceDeviceOrientation];

    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && 8.1 > version) {
        // These suspect versions of iOS will crash if we proceed to recreate the GL view.
        return;
    }

    // We then rebuild the GL view back within the application's event loop.
    dispatch_async(dispatch_get_main_queue(), ^{
        ViewController *controller = (ViewController *)window.rootViewController;
        [controller createView:TRUE];
    });
}

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    return [AppDelegateProxy application:application willFinishLaunchingWithOptions:launchOptions];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [self forceDeviceOrientation];

    // NOTE: On iPhone4 the "resolution" is 480x320 and not 960x640
    // Points vs pixels (and scale factors). I'm not sure that this correct though
    // and that we really get the correct and highest physical resolution in pixels.
    CGRect bounds = [UIScreen mainScreen].bounds;

    window = [[UIWindow alloc] initWithFrame:bounds];
    window.rootViewController = [[[ViewController alloc] init] autorelease];
    [window makeKeyAndVisible];

    g_ApplicationWindow = window;

    // Now, hook in the proxy as the intermediary app delegate
    AppDelegateProxy* proxy = [[AppDelegateProxy alloc] init];

    return [proxy application:application didFinishLaunchingWithOptions:launchOptions];
}

static void ShutdownEngine(bool call_exit)
{
    int run_action = GLFW_APP_RUN_UPDATE;
    int exit_code = 0;
    int argc = 0;
    char** argv = 0;
    g_EngineResultFn(g_EngineContext, &run_action, &exit_code, &argc, &argv);

    g_EngineDestroyFn(g_EngineContext);
    g_EngineContext = 0;

    // Free the old one (if we own it)
    if (g_WasRebooted)
    {
        for (int i = 0; i < g_Argc; ++i)
        {
            free(g_Argv[i]);
        }
        free(g_Argv);
    }

    if (run_action == GLFW_APP_RUN_EXIT)
    {
        NSLog(@"Exiting app with code %d", exit_code);
        if (call_exit)
            exit(exit_code);
        return;
    }

    // GLFW_APP_RUN_REBOOT
    g_Argc = argc;
    g_Argv = argv;
    g_WasRebooted = 1;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    ShutdownEngine(false);
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // We should pause the update loop when this message is sent
    _glfwWin.iconified = GL_TRUE;

    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(0);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    _glfwWin.iconified = GL_FALSE;
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(1);
}

- (void)dealloc
{
    [window release];
    [super dealloc];
}

- (void)appUpdate
{
    if (g_EngineContext == 0)
    {
        g_EngineContext = g_EngineCreateFn(g_Argc, g_Argv);
        if (!g_EngineContext) {
            NSLog(@"Failed to create engine instance.");
            exit(1);
        }
        return;
    }

    int update_code = g_EngineUpdateFn(g_EngineContext);
    if (update_code != 0)
    {
        ShutdownEngine(true);
    }
}

@end

void glfwAppBootstrap(int argc, char** argv, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn)
{
    g_EngineCreateFn = create_fn;
    g_EngineDestroyFn = destroy_fn;
    g_EngineUpdateFn = update_fn;
    g_EngineResultFn = result_fn;
    g_EngineContext = 0;

    g_Argc = argc;
    g_Argv = argv;
    g_WasRebooted = 0;

    int retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    (void) retVal;
}
