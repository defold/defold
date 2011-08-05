//========================================================================
// GLFW - An OpenGL framework
// Platform:    Delphi/FPC + Windows/Linux/Mac OS
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

unit glfw;

// Use standard linking method
// This means links dynamically to glfw.dll on Windows, glfw.so on Linux
// and statically on Mac OS X
//
// If you don't like that, comment this define
{$DEFINE LINK_STANDARD}

// If the define above is commented, remove the dot on your favorite option
{.$DEFINE LINK_DYNAMIC}
{.$DEFINE LINK_STATIC}


{$IFDEF FPC}
  {$MODE DELPHI}
{$ENDIF}

interface

const

  //========================================================================
  // GLFW version
  //========================================================================
  GLFW_VERSION_MAJOR    = 2;
  GLFW_VERSION_MINOR    = 7;
  GLFW_VERSION_REVISION = 0;

  //========================================================================
  // Input handling definitions
  //========================================================================

  // Key and button state/action definitions
  GLFW_RELEASE          = 0;
  GLFW_PRESS            = 1;

  // Keyboard key definitions: 8-bit ISO-8859-1 (Latin 1) encoding is used
  // for printable keys (such as A-Z, 0-9 etc), and values above 256
  // represent special (non-printable) keys (e.g. F1, Page Up etc).
  GLFW_KEY_UNKNOWN      = -1;
  GLFW_KEY_SPACE        = 32;
  GLFW_KEY_SPECIAL      = 256;
  GLFW_KEY_ESC          = (GLFW_KEY_SPECIAL+1);
  GLFW_KEY_F1           = (GLFW_KEY_SPECIAL+2);
  GLFW_KEY_F2           = (GLFW_KEY_SPECIAL+3);
  GLFW_KEY_F3           = (GLFW_KEY_SPECIAL+4);
  GLFW_KEY_F4           = (GLFW_KEY_SPECIAL+5);
  GLFW_KEY_F5           = (GLFW_KEY_SPECIAL+6);
  GLFW_KEY_F6           = (GLFW_KEY_SPECIAL+7);
  GLFW_KEY_F7           = (GLFW_KEY_SPECIAL+8);
  GLFW_KEY_F8           = (GLFW_KEY_SPECIAL+9);
  GLFW_KEY_F9           = (GLFW_KEY_SPECIAL+10);
  GLFW_KEY_F10          = (GLFW_KEY_SPECIAL+11);
  GLFW_KEY_F11          = (GLFW_KEY_SPECIAL+12);
  GLFW_KEY_F12          = (GLFW_KEY_SPECIAL+13);
  GLFW_KEY_F13          = (GLFW_KEY_SPECIAL+14);
  GLFW_KEY_F14          = (GLFW_KEY_SPECIAL+15);
  GLFW_KEY_F15          = (GLFW_KEY_SPECIAL+16);
  GLFW_KEY_F16          = (GLFW_KEY_SPECIAL+17);
  GLFW_KEY_F17          = (GLFW_KEY_SPECIAL+18);
  GLFW_KEY_F18          = (GLFW_KEY_SPECIAL+19);
  GLFW_KEY_F19          = (GLFW_KEY_SPECIAL+20);
  GLFW_KEY_F20          = (GLFW_KEY_SPECIAL+21);
  GLFW_KEY_F21          = (GLFW_KEY_SPECIAL+22);
  GLFW_KEY_F22          = (GLFW_KEY_SPECIAL+23);
  GLFW_KEY_F23          = (GLFW_KEY_SPECIAL+24);
  GLFW_KEY_F24          = (GLFW_KEY_SPECIAL+25);
  GLFW_KEY_F25          = (GLFW_KEY_SPECIAL+26);
  GLFW_KEY_UP           = (GLFW_KEY_SPECIAL+27);
  GLFW_KEY_DOWN         = (GLFW_KEY_SPECIAL+28);
  GLFW_KEY_LEFT         = (GLFW_KEY_SPECIAL+29);
  GLFW_KEY_RIGHT        = (GLFW_KEY_SPECIAL+30);
  GLFW_KEY_LSHIFT       = (GLFW_KEY_SPECIAL+31);
  GLFW_KEY_RSHIFT       = (GLFW_KEY_SPECIAL+32);
  GLFW_KEY_LCTRL        = (GLFW_KEY_SPECIAL+33);
  GLFW_KEY_RCTRL        = (GLFW_KEY_SPECIAL+34);
  GLFW_KEY_LALT         = (GLFW_KEY_SPECIAL+35);
  GLFW_KEY_RALT         = (GLFW_KEY_SPECIAL+36);
  GLFW_KEY_TAB          = (GLFW_KEY_SPECIAL+37);
  GLFW_KEY_ENTER        = (GLFW_KEY_SPECIAL+38);
  GLFW_KEY_BACKSPACE    = (GLFW_KEY_SPECIAL+39);
  GLFW_KEY_INSERT       = (GLFW_KEY_SPECIAL+40);
  GLFW_KEY_DEL          = (GLFW_KEY_SPECIAL+41);
  GLFW_KEY_PAGEUP       = (GLFW_KEY_SPECIAL+42);
  GLFW_KEY_PAGEDOWN     = (GLFW_KEY_SPECIAL+43);
  GLFW_KEY_HOME         = (GLFW_KEY_SPECIAL+44);
  GLFW_KEY_END          = (GLFW_KEY_SPECIAL+45);
  GLFW_KEY_KP_0         = (GLFW_KEY_SPECIAL+46);
  GLFW_KEY_KP_1         = (GLFW_KEY_SPECIAL+47);
  GLFW_KEY_KP_2         = (GLFW_KEY_SPECIAL+48);
  GLFW_KEY_KP_3         = (GLFW_KEY_SPECIAL+49);
  GLFW_KEY_KP_4         = (GLFW_KEY_SPECIAL+50);
  GLFW_KEY_KP_5         = (GLFW_KEY_SPECIAL+51);
  GLFW_KEY_KP_6         = (GLFW_KEY_SPECIAL+52);
  GLFW_KEY_KP_7         = (GLFW_KEY_SPECIAL+53);
  GLFW_KEY_KP_8         = (GLFW_KEY_SPECIAL+54);
  GLFW_KEY_KP_9         = (GLFW_KEY_SPECIAL+55);
  GLFW_KEY_KP_DIVIDE    = (GLFW_KEY_SPECIAL+56);
  GLFW_KEY_KP_MULTIPLY  = (GLFW_KEY_SPECIAL+57);
  GLFW_KEY_KP_SUBTRACT  = (GLFW_KEY_SPECIAL+58);
  GLFW_KEY_KP_ADD       = (GLFW_KEY_SPECIAL+59);
  GLFW_KEY_KP_DECIMAL   = (GLFW_KEY_SPECIAL+60);
  GLFW_KEY_KP_EQUAL     = (GLFW_KEY_SPECIAL+61);
  GLFW_KEY_KP_ENTER     = (GLFW_KEY_SPECIAL+62);
  GLFW_KEY_KP_NUM_LOCK  = (GLFW_KEY_SPECIAL+63);
  GLFW_KEY_CAPS_LOCK    = (GLFW_KEY_SPECIAL+64);
  GLFW_KEY_SCROLL_LOCK  = (GLFW_KEY_SPECIAL+65);  
  GLFW_KEY_PAUSE        = (GLFW_KEY_SPECIAL+66);
  GLFW_KEY_LSUPER       = (GLFW_KEY_SPECIAL+67);
  GLFW_KEY_RSUPER       = (GLFW_KEY_SPECIAL+68);
  GLFW_KEY_MENU         = (GLFW_KEY_SPECIAL+69);
  GLFW_KEY_LAST         = GLFW_KEY_MENU;

  // Mouse button definitions
  GLFW_MOUSE_BUTTON_1      = 0;
  GLFW_MOUSE_BUTTON_2      = 1;
  GLFW_MOUSE_BUTTON_3      = 2;
  GLFW_MOUSE_BUTTON_4      = 3;
  GLFW_MOUSE_BUTTON_5      = 4;
  GLFW_MOUSE_BUTTON_6      = 5;
  GLFW_MOUSE_BUTTON_7      = 6;
  GLFW_MOUSE_BUTTON_8      = 7;
  GLFW_MOUSE_BUTTON_LAST   = GLFW_MOUSE_BUTTON_8;

  // Mouse button aliases
  GLFW_MOUSE_BUTTON_LEFT   = GLFW_MOUSE_BUTTON_1;
  GLFW_MOUSE_BUTTON_RIGHT  = GLFW_MOUSE_BUTTON_2;
  GLFW_MOUSE_BUTTON_MIDDLE = GLFW_MOUSE_BUTTON_3;

  // Joystick identifiers
  GLFW_JOYSTICK_1           = 0;
  GLFW_JOYSTICK_2           = 1;
  GLFW_JOYSTICK_3           = 2;
  GLFW_JOYSTICK_4           = 3;
  GLFW_JOYSTICK_5           = 4;
  GLFW_JOYSTICK_6           = 5;
  GLFW_JOYSTICK_7           = 6;
  GLFW_JOYSTICK_8           = 7;
  GLFW_JOYSTICK_9           = 8;
  GLFW_JOYSTICK_10          = 9;
  GLFW_JOYSTICK_11          = 10;
  GLFW_JOYSTICK_12          = 11;
  GLFW_JOYSTICK_13          = 12;
  GLFW_JOYSTICK_14          = 13;
  GLFW_JOYSTICK_15          = 14;
  GLFW_JOYSTICK_16          = 15;
  GLFW_JOYSTICK_LAST        = GLFW_JOYSTICK_16;


  //========================================================================
  // Other definitions
  //========================================================================

  // glfwOpenWindow modes
  GLFW_WINDOW               = $00010001;
  GLFW_FULLSCREEN           = $00010002;

  // glfwGetWindowParam tokens
  GLFW_OPENED               = $00020001;
  GLFW_ACTIVE               = $00020002;
  GLFW_ICONIFIED            = $00020003;
  GLFW_ACCELERATED          = $00020004;
  GLFW_RED_BITS             = $00020005;
  GLFW_GREEN_BITS           = $00020006;
  GLFW_BLUE_BITS            = $00020007;
  GLFW_ALPHA_BITS           = $00020008;
  GLFW_DEPTH_BITS           = $00020009;
  GLFW_STENCIL_BITS         = $0002000A;

  // The following constants are used for both glfwGetWindowParam
  // and glfwOpenWindowHint
  GLFW_REFRESH_RATE         = $0002000B;
  GLFW_ACCUM_RED_BITS       = $0002000C;
  GLFW_ACCUM_GREEN_BITS     = $0002000D;
  GLFW_ACCUM_BLUE_BITS      = $0002000E;
  GLFW_ACCUM_ALPHA_BITS     = $0002000F;
  GLFW_AUX_BUFFERS          = $00020010;
  GLFW_STEREO               = $00020011;
  GLFW_WINDOW_NO_RESIZE     = $00020012;
  GLFW_FSAA_SAMPLES         = $00020013;
  GLFW_OPENGL_VERSION_MAJOR = $00020014;
  GLFW_OPENGL_VERSION_MINOR = $00020015;
  GLFW_OPENGL_FORWARD_COMPAT= $00020016;
  GLFW_DEBUG_CONTEXT        = $00020017;
  GLFW_OPENGL_PROFILE       = $00020018;

  // GLFW_OPENGL_PROFILE tokens
  GLFW_OPENGL_CORE_PROFILE  = $00050001;
  GLFW_OPENGL_COMPAT_PROFILE = $00050002;

  // glfwEnable/glfwDisable tokens
  GLFW_MOUSE_CURSOR         = $00030001;
  GLFW_STICKY_KEYS          = $00030002;
  GLFW_STICKY_MOUSE_BUTTONS = $00030003;
  GLFW_SYSTEM_KEYS          = $00030004;
  GLFW_KEY_REPEAT           = $00030005;
  GLFW_AUTO_POLL_EVENTS     = $00030006;

  // glfwWaitThread wait modes
  GLFW_WAIT                 = $00040001;
  GLFW_NOWAIT               = $00040002;

  // glfwGetJoystickParam tokens
  GLFW_PRESENT              = $00050001;
  GLFW_AXES                 = $00050002;
  GLFW_BUTTONS              = $00050003;

  // glfwReadImage/glfwLoadTexture2D flags
  GLFW_NO_RESCALE_BIT       = $00000001; // Only for glfwReadImage
  GLFW_ORIGIN_UL_BIT        = $00000002;
  GLFW_BUILD_MIPMAPS_BIT    = $00000004; // Only for glfwLoadTexture2D
  GLFW_ALPHA_MAP_BIT        = $00000008;

  // Time spans longer than this (seconds) are considered to be infinity
  GLFW_INFINITY             = 100000.0;


  //========================================================================
  // Typedefs
  //========================================================================

type

  // The video mode structure used by glfwGetVideoModes()
  GLFWvidmode = packed record
    Width, Height               : Integer;
    RedBits, BlueBits, GreenBits: Integer;
  end;
  PGLFWvidmode = ^GLFWvidmode;

  // Image/texture information
  GLFWimage = packed record
    Width, Height: Integer;
    Format       : Integer;
    BytesPerPixel: Integer;
    Data         : PChar;
  end;
  PGLFWimage = ^GLFWimage;

  // Thread ID
  GLFWthread = Integer;

  // Mutex object
  GLFWmutex = Pointer;

  // Condition variable object
  GLFWcond = Pointer;

  // Function pointer types
  GLFWwindowsizefun    = procedure(Width, Height: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWwindowclosefun   = function: Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWwindowrefreshfun = procedure; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWmousebuttonfun   = procedure(Button, Action: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWmouseposfun      = procedure(X, Y: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWmousewheelfun    = procedure(Pos: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWkeyfun           = procedure(Key, Action: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWcharfun          = procedure(Character, Action: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}
  GLFWthreadfun        = procedure(Arg: Pointer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF}


//========================================================================
// Prototypes
//========================================================================

{$IFDEF LINK_DYNAMIC}
const
  {$IFDEF WIN32}
    DLLNAME = 'glfw.dll';
  {$ELSE}
    {$IFDEF DARWIN}
    DLLNAME = 'libglfw.dylib';
    {$ELSE}
    DLLNAME = 'libglfw.so';
    {$ENDIF}
  {$ENDIF}
{$ENDIF}


// GLFW initialization, termination and version querying
function  glfwInit: Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwTerminate; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwGetVersion(var major: Integer; var minor: Integer; var Rev: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Window handling
function  glfwOpenWindow(width, height, redbits, greenbits, bluebits, alphabits, depthbits, stencilbits, mode: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwOpenWindowHint(target, hint: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwCloseWindow; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowTitle(title: PChar); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwGetWindowSize(var width: Integer; var height: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowSize(width, height: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowPos(x, y: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwIconifyWindow; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwRestoreWindow; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSwapBuffers; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSwapInterval(interval: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetWindowParam(Param: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowSizeCallback(cbfun: GLFWwindowsizefun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowCloseCallback(cbfun: GLFWwindowclosefun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetWindowRefreshCallback(cbfun: GLFWwindowrefreshfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Video mode functions
function  glfwGetVideoModes(list: PGLFWvidmode; maxcount: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwGetDesktopMode(mode: PGLFWvidmode); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Input handling
procedure glfwPollEvents; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwWaitEvents; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetKey(key: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetMouseButton(button: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwGetMousePos(var xpos: Integer; var ypos: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetMousePos(xpos, ypos: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetMouseWheel: Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetMouseWheel(pos: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetKeyCallback( cbfun: GLFWkeyfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetCharCallback( cbfun: GLFWcharfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetMouseButtonCallback( cbfun: GLFWmousebuttonfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetMousePosCallback( cbfun: GLFWmouseposfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetMouseWheelCallback( cbfun: GLFWmousewheelfun); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Joystick input
function  glfwGetJoystickParam(joy, param: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetJoystickPos(joy: Integer; var pos: Single; numaxes: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetJoystickButtons(joy: Integer; buttons: PChar; numbuttons: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Time
function  glfwGetTime: Double; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSetTime(time: Double); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSleep(time: Double); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Extension support
function  glfwExtensionSupported(extension: PChar): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetProcAddress(procname: PChar): Pointer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwGetGLVersion(var major: Integer; var minor: Integer; var Rev: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Threading support
function  glfwCreateThread(fun: GLFWthreadfun; arg: Pointer): GLFWthread; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwDestroyThread(Id: GLFWthread); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwWaitThread(Id: GLFWthread; waitmode: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetThreadID: GLFWthread; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwCreateMutex: GLFWmutex; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwDestroyMutex( mutex: GLFWmutex); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwLockMutex(mutex: GLFWmutex); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwUnlockMutex(mutex: GLFWmutex); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwCreateCond: GLFWcond; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwDestroyCond(cond: GLFWcond); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwWaitCond(cond: GLFWcond; mutex: GLFWmutex; timeout: Double); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwSignalCond(cond: GLFWcond); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwBroadcastCond(cond: GLFWcond); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwGetNumberOfProcessors: Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Enable/disable functions
procedure glfwEnable(token: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwDisable(token: Integer); {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

// Image/texture I/O support
function  glfwReadImage(name: PChar; image: PGLFWimage; flags: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwReadMemoryImage(data: Pointer; size: LongInt; img: PGLFWimage; flags: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
procedure glfwFreeImage(img: PGLFWimage);  {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwLoadTexture2D(name: PChar; flags: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwLoadMemoryTexture2D(data: Pointer; size: LongInt; flags: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};
function  glfwLoadTextureImage2D(img: PGLFWimage; flags: Integer): Integer; {$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external {$IFDEF LINK_DYNAMIC} DLLNAME {$ENDIF};

implementation



end.
