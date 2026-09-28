/************************************************************************
 * GLFW - An OpenGL framework
 * API version: 2.7
 * WWW:         http://www.glfw.org/
 *------------------------------------------------------------------------
 * Copyright (c) 2002-2006 Marcus Geelnard
 * Copyright (c) 2006-2010 Camilla Berglund
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would
 *    be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 *
 *************************************************************************/
// Modified for Defold: private mobile/web backend, without the GLFW API.


#ifndef DM_PLATFORM_NATIVE_H
#define DM_PLATFORM_NATIVE_H

#include <stdint.h>
#include "native_touch.h"
#include "../dmsdk/platform/window.h"
#if defined(DM_PLATFORM_IOS)
#include <OpenGLES/ES3/gl.h>
#elif defined(ANDROID)
#include <EGL/egl.h>
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Key and button state/action definitions */
#define NATIVE_RELEASE            0
#define NATIVE_PRESS              1

#define NATIVE_HAT_CENTERED       0
#define NATIVE_HAT_UP             1
#define NATIVE_HAT_RIGHT          2
#define NATIVE_HAT_DOWN           4
#define NATIVE_HAT_LEFT           8


/* Keyboard key definitions: 8-bit ISO-8859-1 (Latin 1) encoding is used
 * for printable keys (such as A-Z, 0-9 etc), and values above 256
 * represent special (non-printable) keys (e.g. F1, Page Up etc).
 */
#define NATIVE_KEY_SPACE        32
#define NATIVE_KEY_SPECIAL      256
#define NATIVE_KEY_ESC          (NATIVE_KEY_SPECIAL+1)
#define NATIVE_KEY_F1           (NATIVE_KEY_SPECIAL+2)
#define NATIVE_KEY_F2           (NATIVE_KEY_SPECIAL+3)
#define NATIVE_KEY_F3           (NATIVE_KEY_SPECIAL+4)
#define NATIVE_KEY_F4           (NATIVE_KEY_SPECIAL+5)
#define NATIVE_KEY_F5           (NATIVE_KEY_SPECIAL+6)
#define NATIVE_KEY_F6           (NATIVE_KEY_SPECIAL+7)
#define NATIVE_KEY_F7           (NATIVE_KEY_SPECIAL+8)
#define NATIVE_KEY_F8           (NATIVE_KEY_SPECIAL+9)
#define NATIVE_KEY_F9           (NATIVE_KEY_SPECIAL+10)
#define NATIVE_KEY_F10          (NATIVE_KEY_SPECIAL+11)
#define NATIVE_KEY_F11          (NATIVE_KEY_SPECIAL+12)
#define NATIVE_KEY_F12          (NATIVE_KEY_SPECIAL+13)
#define NATIVE_KEY_UP           (NATIVE_KEY_SPECIAL+27)
#define NATIVE_KEY_DOWN         (NATIVE_KEY_SPECIAL+28)
#define NATIVE_KEY_LEFT         (NATIVE_KEY_SPECIAL+29)
#define NATIVE_KEY_RIGHT        (NATIVE_KEY_SPECIAL+30)
#define NATIVE_KEY_LSHIFT       (NATIVE_KEY_SPECIAL+31)
#define NATIVE_KEY_RSHIFT       (NATIVE_KEY_SPECIAL+32)
#define NATIVE_KEY_LCTRL        (NATIVE_KEY_SPECIAL+33)
#define NATIVE_KEY_RCTRL        (NATIVE_KEY_SPECIAL+34)
#define NATIVE_KEY_LALT         (NATIVE_KEY_SPECIAL+35)
#define NATIVE_KEY_RALT         (NATIVE_KEY_SPECIAL+36)
#define NATIVE_KEY_TAB          (NATIVE_KEY_SPECIAL+37)
#define NATIVE_KEY_ENTER        (NATIVE_KEY_SPECIAL+38)
#define NATIVE_KEY_BACKSPACE    (NATIVE_KEY_SPECIAL+39)
#define NATIVE_KEY_INSERT       (NATIVE_KEY_SPECIAL+40)
#define NATIVE_KEY_DEL          (NATIVE_KEY_SPECIAL+41)
#define NATIVE_KEY_PAGEUP       (NATIVE_KEY_SPECIAL+42)
#define NATIVE_KEY_PAGEDOWN     (NATIVE_KEY_SPECIAL+43)
#define NATIVE_KEY_HOME         (NATIVE_KEY_SPECIAL+44)
#define NATIVE_KEY_END          (NATIVE_KEY_SPECIAL+45)
#define NATIVE_KEY_KP_0         (NATIVE_KEY_SPECIAL+46)
#define NATIVE_KEY_KP_1         (NATIVE_KEY_SPECIAL+47)
#define NATIVE_KEY_KP_2         (NATIVE_KEY_SPECIAL+48)
#define NATIVE_KEY_KP_3         (NATIVE_KEY_SPECIAL+49)
#define NATIVE_KEY_KP_4         (NATIVE_KEY_SPECIAL+50)
#define NATIVE_KEY_KP_5         (NATIVE_KEY_SPECIAL+51)
#define NATIVE_KEY_KP_6         (NATIVE_KEY_SPECIAL+52)
#define NATIVE_KEY_KP_7         (NATIVE_KEY_SPECIAL+53)
#define NATIVE_KEY_KP_8         (NATIVE_KEY_SPECIAL+54)
#define NATIVE_KEY_KP_9         (NATIVE_KEY_SPECIAL+55)
#define NATIVE_KEY_KP_DIVIDE    (NATIVE_KEY_SPECIAL+56)
#define NATIVE_KEY_KP_MULTIPLY  (NATIVE_KEY_SPECIAL+57)
#define NATIVE_KEY_KP_SUBTRACT  (NATIVE_KEY_SPECIAL+58)
#define NATIVE_KEY_KP_ADD       (NATIVE_KEY_SPECIAL+59)
#define NATIVE_KEY_KP_DECIMAL   (NATIVE_KEY_SPECIAL+60)
#define NATIVE_KEY_KP_EQUAL     (NATIVE_KEY_SPECIAL+61)
#define NATIVE_KEY_KP_ENTER     (NATIVE_KEY_SPECIAL+62)
#define NATIVE_KEY_KP_NUM_LOCK  (NATIVE_KEY_SPECIAL+63)
#define NATIVE_KEY_CAPS_LOCK    (NATIVE_KEY_SPECIAL+64)
#define NATIVE_KEY_SCROLL_LOCK  (NATIVE_KEY_SPECIAL+65)
#define NATIVE_KEY_PAUSE        (NATIVE_KEY_SPECIAL+66)
#define NATIVE_KEY_LSUPER       (NATIVE_KEY_SPECIAL+67)
#define NATIVE_KEY_RSUPER       (NATIVE_KEY_SPECIAL+68)
#define NATIVE_KEY_MENU         (NATIVE_KEY_SPECIAL+69)
#define NATIVE_KEY_BACK         (NATIVE_KEY_SPECIAL+70)
#define NATIVE_KEY_LAST         NATIVE_KEY_BACK

#define NATIVE_KEYBOARD_DEFAULT    (0)
#define NATIVE_KEYBOARD_NUMBER_PAD (1)
#define NATIVE_KEYBOARD_EMAIL      (2)
#define NATIVE_KEYBOARD_PASSWORD   (3)

/* Mouse button definitions */
#define NATIVE_MOUSE_BUTTON_1      0
#define NATIVE_MOUSE_BUTTON_2      1
#define NATIVE_MOUSE_BUTTON_3      2
#define NATIVE_MOUSE_BUTTON_4      3
#define NATIVE_MOUSE_BUTTON_5      4
#define NATIVE_MOUSE_BUTTON_6      5
#define NATIVE_MOUSE_BUTTON_7      6
#define NATIVE_MOUSE_BUTTON_8      7
#define NATIVE_MOUSE_BUTTON_LAST   NATIVE_MOUSE_BUTTON_8

/* Mouse button aliases */
#define NATIVE_MOUSE_BUTTON_LEFT   NATIVE_MOUSE_BUTTON_1
#define NATIVE_MOUSE_BUTTON_RIGHT  NATIVE_MOUSE_BUTTON_2
#define NATIVE_MOUSE_BUTTON_MIDDLE NATIVE_MOUSE_BUTTON_3


/* Joystick identifiers */
#define NATIVE_JOYSTICK_1          0
#define NATIVE_JOYSTICK_2          1
#define NATIVE_JOYSTICK_3          2
#define NATIVE_JOYSTICK_4          3
#define NATIVE_JOYSTICK_5          4
#define NATIVE_JOYSTICK_6          5
#define NATIVE_JOYSTICK_7          6
#define NATIVE_JOYSTICK_8          7
#define NATIVE_JOYSTICK_9          8
#define NATIVE_JOYSTICK_10         9
#define NATIVE_JOYSTICK_11         10
#define NATIVE_JOYSTICK_12         11
#define NATIVE_JOYSTICK_13         12
#define NATIVE_JOYSTICK_14         13
#define NATIVE_JOYSTICK_15         14
#define NATIVE_JOYSTICK_16         15
#define NATIVE_JOYSTICK_LAST       NATIVE_JOYSTICK_16


/*************************************************************************
 * Other definitions
 *************************************************************************/

/* dmNativeOpenWindow modes */
#define NATIVE_WINDOW               0x00010001
#define NATIVE_FULLSCREEN           0x00010002

/* dmNativeGetWindowParam tokens */
#define NATIVE_OPENED               0x00020001
#define NATIVE_ACTIVE               0x00020002
#define NATIVE_ICONIFIED            0x00020003
#define NATIVE_ACCELERATED          0x00020004
#define NATIVE_RED_BITS             0x00020005
#define NATIVE_GREEN_BITS           0x00020006
#define NATIVE_BLUE_BITS            0x00020007
#define NATIVE_ALPHA_BITS           0x00020008
#define NATIVE_DEPTH_BITS           0x00020009
#define NATIVE_STENCIL_BITS         0x0002000A

/* The following constants are used for both dmNativeGetWindowParam
 * and dmNativeOpenWindowHint
 */
#define NATIVE_REFRESH_RATE         0x0002000B
#define NATIVE_ACCUM_RED_BITS       0x0002000C
#define NATIVE_ACCUM_GREEN_BITS     0x0002000D
#define NATIVE_ACCUM_BLUE_BITS      0x0002000E
#define NATIVE_ACCUM_ALPHA_BITS     0x0002000F
#define NATIVE_AUX_BUFFERS          0x00020010
#define NATIVE_STEREO               0x00020011
#define NATIVE_WINDOW_NO_RESIZE     0x00020012
#define NATIVE_FSAA_SAMPLES         0x00020013
#define NATIVE_OPENGL_PROFILE       0x00020018
#define NATIVE_WINDOW_HIGH_DPI      0x00020019
#define NATIVE_CLIENT_API           0x0002001A

/* NATIVE_OPENGL_PROFILE tokens */
#define NATIVE_OPENGL_CORE_PROFILE  0x00050001
#define NATIVE_OPENGL_COMPAT_PROFILE 0x00050002

/* dmNativeEnable/dmNativeDisable tokens */
#define NATIVE_MOUSE_CURSOR         0x00030001
#define NATIVE_STICKY_KEYS          0x00030002
#define NATIVE_STICKY_MOUSE_BUTTONS 0x00030003
#define NATIVE_SYSTEM_KEYS          0x00030004
#define NATIVE_KEY_REPEAT           0x00030005
#define NATIVE_AUTO_POLL_EVENTS     0x00030006

/* NATIVE_CLIENT_API modes */
#define NATIVE_NO_API                        0
#define NATIVE_OPENGL_API           0x00030001

/* dmNativeWaitThread wait modes */

/* dmNativeGetJoystickParam tokens */
#define NATIVE_PRESENT              0x00050001
#define NATIVE_AXES                 0x00050002
#define NATIVE_BUTTONS              0x00050003
#define NATIVE_HATS                 0x00050004

/* dmNativeReadImage/dmNativeLoadTexture2D flags */

/* Time spans longer than this (seconds) are considered to be infinity */

/*************************************************************************
 * Typedefs
 *************************************************************************/



/* Function pointer types */
typedef void (* Nativewindowsizefun)(int,int);
typedef int  (* Nativewindowclosefun)(void);
typedef void (* Nativewindowrefreshfun)(void);
typedef void (* Nativemousebuttonfun)(int,int);
typedef void (* Nativemouseposfun)(int,int);
typedef void (* Nativemousewheelfun)(int);
typedef void (* Nativekeyfun)(int,int);
typedef void (* Nativecharfun)(int,int);
typedef void (* Nativemarkedtextfun)(char *);
typedef void (* Nativegamepadfun)(int,int);
typedef void (* Nativedevicechangedfun)(int);
typedef void (* Nativetouchfun)(int,int,int,int);


/*************************************************************************
 * Prototypes
 *************************************************************************/

/* Native initialization, termination and version querying */
int  dmNativeInit( void );
void dmNativeTerminate( void );

/* Window handling */
int dmNativeOpenWindow(const WindowCreateParams* params);
void dmNativeSetCursorVisible(int visible);
void dmNativeCloseWindow( void );
int  dmNativeGetDefaultFramebuffer( void );
void dmNativeSetWindowTitle( const char *title );
void dmNativeGetWindowSize( int *width, int *height );
void dmNativeSetWindowSize( int width, int height );
void dmNativeSetWindowPos( int x, int y );
void dmNativeIconifyWindow( void );
void dmNativeSwapBuffers( void );
void dmNativeSwapInterval( int interval );
int  dmNativeGetWindowParam( WindowState param );
void dmNativeSetWindowSizeCallback( Nativewindowsizefun cbfun );
void dmNativeSetWindowCloseCallback( Nativewindowclosefun cbfun );
int  dmNativeGetWindowRefreshRate( void );

/* Video mode functions */

/* Input handling */
void dmNativePollEvents( void );
int  dmNativeGetKey( int key );
int  dmNativeGetMouseButton( int button );
void dmNativeGetMousePos( int *xpos, int *ypos );
int  dmNativeGetMouseWheel( void );
int  dmNativeSetCharCallback( Nativecharfun cbfun );
int  dmNativeSetMarkedTextCallback( Nativemarkedtextfun cbfun );
void dmNativeShowKeyboard( int show, int type, int auto_close );
void dmNativeResetKeyboard( void );
int  dmNativeGetTouch(NativeTouch* touch, int count, int* out_count);
int  dmNativeSetTouchCallback( Nativetouchfun cbfun );

int dmNativeGetAcceleration(float* x, float* y, float* z);

/* Joystick input */
#define NATIVE_JOYSTICK_DEVICE_GUID_LENGTH 32

void dmNativeCreateJoystickDeviceGuid( unsigned short bus, unsigned short vendor, unsigned short product, unsigned short version, const char* vendor_name, const char* product_name, unsigned char driver_signature, unsigned char driver_data, char guid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH + 1] );
int dmNativeGetJoystickParam( int joy, int param );
int dmNativeGetJoystickPos( int joy, float *pos, int numaxes );
int dmNativeGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons );
int dmNativeGetJoystickHats( int joy, unsigned char *hats, int numhats );
int dmNativeGetJoystickDeviceId( int joy, char** device_id );
int dmNativeGetJoystickDeviceGuid( int joy, char** device_guid );

/* Time */

/* Extension support */
void* dmNativeGetProcAddress( const char *procname );

/* Threading support */

/* Enable/disable functions */

/* Image/texture I/O support */

// Defold extensions
void dmNativeRegisterUIApplicationDelegate(void* delegate);
void dmNativeUnregisterUIApplicationDelegate(void* delegate);
void dmNativeRegisterUISceneDelegate(void* delegate);
void dmNativeUnregisterUISceneDelegate(void* delegate);
void dmNativeSetViewType(int view_type);
void dmNativeSetWindowBackgroundColor(unsigned int color);
float dmNativeGetDisplayScaleFactor();
int dmNativeGetMouseLocked();

// Defold extensions (Android)
#if defined(ANDROID)
void    dmNativeAndroidBeginFrame();
void    dmNativeAndroidHandleCommand(struct android_app* app, int32_t cmd);
int32_t dmNativeAndroidHandleInput(struct android_app* app, struct AInputEvent* event);
int32_t dmNativeAndroidWindowOpened();
void    dmNativeAndroidPollEvents();
void    dmNativeAndroidFlushEvents();
int32_t dmNativeAndroidVerifySurface();

// Activity control
typedef void (*dmNativeactivityresultfun)(void *env, void* activity, int request_code, int result_code, void* result);
void dmNativeAndroidRegisterOnActivityResultListener(dmNativeactivityresultfun fun);
void dmNativeAndroidUnregisterOnActivityResultListener(dmNativeactivityresultfun fun);

// onCreate listeners
typedef void (*dmNativeoncreatefun)(void *env, void* activity);
void dmNativeAndroidRegisterOnCreateListener(dmNativeoncreatefun fun);
void dmNativeAndroidUnregisterOnCreateListener(dmNativeoncreatefun fun);
#endif

// Accelerometer control
void dmNativeAccelerometerEnable();

// context control
int   dmNativeQueryAuxContext();
void* dmNativeAcquireAuxContext();
void  dmNativeUnacquireAuxContext(void* context);

// Trying to mimic somewhat the features of dmNative 3.0
typedef void (* Nativewindowfocusfun)(int);
typedef void (* Nativewindowiconifyfun)(int);
void dmNativeSetWindowFocusCallback( Nativewindowfocusfun cbfun );
void dmNativeSetWindowIconifyCallback( Nativewindowiconifyfun cbfun );
int  dmNativeSetGamepadCallback( Nativegamepadfun cbfun );
int  dmNativeSetDeviceChangedCallback( Nativedevicechangedfun cbfun );

#ifdef __cplusplus
}
#endif

#endif /* _dmNative_h_ */
