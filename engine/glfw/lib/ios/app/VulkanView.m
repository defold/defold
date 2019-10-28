#include "VulkanView.h"
#import "TextUtil.h"
#import "AppDelegate.h"
#import <QuartzCore/CAMetalLayer.h>

#include "internal.h"

_GLFWwin          		g_Savewin;

// AppDelegate.m
extern UIWindow*        g_ApplicationWindow;
extern AppDelegate*     g_AppDelegate;

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

void* _glfwPlatformAcquireAuxContext()
{
}

void _glfwPlatformUnacquireAuxContext(void* context)
{
}

int _glfwPlatformQueryAuxContext()
{
    return 0;
}

int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    _glfwWin.width = [g_VulkanView getWindowWidth];
    _glfwWin.height = [g_VulkanView getWindowHeight];

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;

    _glfwWin.pixelFormat = nil;
    _glfwWin.delegate = g_AppDelegate;

    _glfwWin.view = g_VulkanView;
    _glfwWin.window = g_ApplicationWindow;

    // opengl
    _glfwWin.context = nil;
    _glfwWin.aux_context = nil;

	// vulkan
    _glfwWin.clientAPI = GLFW_NO_API;

    return GL_TRUE;
}
