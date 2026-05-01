// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_PLATFORM_WINDOW_H
#define DMSDK_PLATFORM_WINDOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*# Window API documentation
 *
 * Window creation and event polling.
 *
 * @document
 * @name Window
 * @language C
 */

/*# Window handle
 * @typedef
 * @name HWindow
 */
typedef struct dmWindow* HWindow;

/*# window resize callback
 * Called when the window size changes.
 * @typedef
 * @name FWindowResizeCallback
 */
typedef void (*FWindowResizeCallback)(void* user_data, uint32_t width, uint32_t height);

/*# window focus callback
 * Called when the window focus changes.
 * @typedef
 * @name FWindowFocusCallback
 */
typedef void (*FWindowFocusCallback)(void* user_data, uint32_t focus);

/*# window iconify callback
 * Called when the window is iconified/minimized or restored.
 * @typedef
 * @name FWindowIconifyCallback
 */
typedef void (*FWindowIconifyCallback)(void* user_data, uint32_t iconified);

/*# window close callback
 * Called when the window is requested to close.
 * Return non-zero to allow close, or zero to cancel close.
 * @typedef
 * @name FWindowCloseCallback
 */
typedef int (*FWindowCloseCallback)(void* user_data);

/*# keyboard char callback
 * Called when a character is entered from keyboard input.
 * @typedef
 * @name FWindowAddKeyboardCharCallback
 */
typedef void (*FWindowAddKeyboardCharCallback)(void* user_data, int chr);

/*# marked text callback
 * Called when IME marked text is updated.
 * @typedef
 * @name FWindowSetMarkedTextCallback
 */
typedef void (*FWindowSetMarkedTextCallback)(void* user_data, char* text);

/*# device changed callback
 * Called when keyboard input device status changes.
 * @typedef
 * @name FWindowDeviceChangedCallback
 */
typedef void (*FWindowDeviceChangedCallback)(void* user_data, int status);

/*# result enumeration
 * @enum
 * @name WindowResult
 * @member WINDOW_RESULT_OK Operation completed successfully
 * @member WINDOW_RESULT_WINDOW_OPEN_ERROR Failed to open the window
 * @member WINDOW_RESULT_WINDOW_ALREADY_OPENED Window is already open
 */
typedef enum WindowResult
{
    WINDOW_RESULT_OK                    = 0,
    WINDOW_RESULT_WINDOW_OPEN_ERROR     = -1,
    WINDOW_RESULT_WINDOW_ALREADY_OPENED = -2,
} WindowResult;

/*# graphics api enumeration
 * @enum
 * @name WindowsGraphicsApi
 * @member WINDOW_GRAPHICS_API_NULL No graphics backend
 * @member WINDOW_GRAPHICS_API_OPENGL OpenGL backend
 * @member WINDOW_GRAPHICS_API_OPENGLES OpenGL ES backend
 * @member WINDOW_GRAPHICS_API_VULKAN Vulkan backend
 * @member WINDOW_GRAPHICS_API_VENDOR Vendor-specific backend
 * @member WINDOW_GRAPHICS_API_WEBGPU WebGPU backend
 * @member WINDOW_GRAPHICS_API_DIRECTX DirectX backend
 */
typedef enum WindowsGraphicsApi
{
    WINDOW_GRAPHICS_API_NULL     = 0,
    WINDOW_GRAPHICS_API_OPENGL   = 1,
    WINDOW_GRAPHICS_API_OPENGLES = 2,
    WINDOW_GRAPHICS_API_VULKAN   = 3,
    WINDOW_GRAPHICS_API_VENDOR   = 4,
    WINDOW_GRAPHICS_API_WEBGPU   = 5,
    WINDOW_GRAPHICS_API_DIRECTX  = 6,
} WindowsGraphicsApi;

/*# window state enumeration
 * @enum
 * @name WindowState
 * @member WINDOW_STATE_OPENED Window is opened
 * @member WINDOW_STATE_ACTIVE Window is active/focused
 * @member WINDOW_STATE_ICONIFIED Window is iconified/minimized
 * @member WINDOW_STATE_ACCELERATED Window has an accelerated graphics context
 * @member WINDOW_STATE_RED_BITS Red channel bit depth
 * @member WINDOW_STATE_GREEN_BITS Green channel bit depth
 * @member WINDOW_STATE_BLUE_BITS Blue channel bit depth
 * @member WINDOW_STATE_ALPHA_BITS Alpha channel bit depth
 * @member WINDOW_STATE_DEPTH_BITS Depth buffer bit depth
 * @member WINDOW_STATE_STENCIL_BITS Stencil buffer bit depth
 * @member WINDOW_STATE_REFRESH_RATE Display refresh rate
 * @member WINDOW_STATE_ACCUM_RED_BITS Accumulation red channel bit depth
 * @member WINDOW_STATE_ACCUM_GREEN_BITS Accumulation green channel bit depth
 * @member WINDOW_STATE_ACCUM_BLUE_BITS Accumulation blue channel bit depth
 * @member WINDOW_STATE_ACCUM_ALPHA_BITS Accumulation alpha channel bit depth
 * @member WINDOW_STATE_AUX_BUFFERS Number of auxiliary buffers
 * @member WINDOW_STATE_STEREO Stereo rendering enabled
 * @member WINDOW_STATE_WINDOW_NO_RESIZE Window resize disabled
 * @member WINDOW_STATE_FSAA_SAMPLES Requested FSAA sample count
 * @member WINDOW_STATE_SAMPLE_COUNT Actual sample count used by the window/context
 * @member WINDOW_STATE_HIGH_DPI High-DPI framebuffer enabled
 * @member WINDOW_STATE_AUX_CONTEXT Auxiliary graphics context supported
 */
typedef enum WindowState
{
    WINDOW_STATE_OPENED             = 1,
    WINDOW_STATE_ACTIVE             = 2,
    WINDOW_STATE_ICONIFIED          = 3,
    WINDOW_STATE_ACCELERATED        = 4,
    WINDOW_STATE_RED_BITS           = 5,
    WINDOW_STATE_GREEN_BITS         = 6,
    WINDOW_STATE_BLUE_BITS          = 7,
    WINDOW_STATE_ALPHA_BITS         = 8,
    WINDOW_STATE_DEPTH_BITS         = 9,
    WINDOW_STATE_STENCIL_BITS       = 10,
    WINDOW_STATE_REFRESH_RATE       = 11,
    WINDOW_STATE_ACCUM_RED_BITS     = 12,
    WINDOW_STATE_ACCUM_GREEN_BITS   = 13,
    WINDOW_STATE_ACCUM_BLUE_BITS    = 14,
    WINDOW_STATE_ACCUM_ALPHA_BITS   = 15,
    WINDOW_STATE_AUX_BUFFERS        = 16,
    WINDOW_STATE_STEREO             = 17,
    WINDOW_STATE_WINDOW_NO_RESIZE   = 18,
    WINDOW_STATE_FSAA_SAMPLES       = 19,
    WINDOW_STATE_SAMPLE_COUNT       = 20,
    WINDOW_STATE_HIGH_DPI           = 21,
    WINDOW_STATE_AUX_CONTEXT        = 22,
} WindowState;

/*# window parameters
 * @struct
 * @name WindowCreateParams
 * @member m_GraphicsApi [type:WindowsGraphicsApi] Graphics backend to request when opening the window
 * @member m_ResizeCallback [type:FWindowResizeCallback] Callback invoked when the window size changes
 * @member m_ResizeCallbackUserData [type:void*] User data pointer passed to m_ResizeCallback
 * @member m_CloseCallback [type:FWindowCloseCallback] Callback invoked when the window is requested to close
 * @member m_CloseCallbackUserData [type:void*] User data pointer passed to m_CloseCallback
 * @member m_FocusCallback [type:FWindowFocusCallback] Callback invoked when focus changes
 * @member m_FocusCallbackUserData [type:void*] User data pointer passed to m_FocusCallback
 * @member m_IconifyCallback [type:FWindowIconifyCallback] Callback invoked when iconify/minimize state changes
 * @member m_IconifyCallbackUserData [type:void*] User data pointer passed to m_IconifyCallback
 * @member m_Title [type:const char*] Window title (default: "Defold Application")
 * @member m_Width [type:uint32_t] Initial window width in pixels (default: 640)
 * @member m_Height [type:uint32_t] Initial window height in pixels (default: 480)
 * @member m_Samples [type:uint32_t] Requested multisample anti-aliasing sample count (default: 1)
 * @member m_BackgroundColor [type:uint32_t] Initial window background color value
 * @member m_ContextAlphabits [type:uint8_t] Requested alpha bits for the graphics context
 * @member m_OpenGLVersionHint [type:uint8_t:7] OpenGL version hint encoded as major*10 + minor (for example 33 means OpenGL 3.3)
 * @member m_OpenGLUseCoreProfileHint [type:uint8_t:1] Request OpenGL core profile when opening the window
 * @member m_Hidden [type:uint8_t:1] Start window hidden
 * @member m_Fullscreen [type:uint8_t:1] Start window in fullscreen mode
 * @member m_PrintDeviceInfo [type:uint8_t:1] Print graphics device information when opening the window
 * @member m_HighDPI [type:uint8_t:1] Request high-DPI framebuffer support where available
 */
typedef struct WindowCreateParams
{
    WindowsGraphicsApi      m_GraphicsApi;
    FWindowResizeCallback   m_ResizeCallback;
    void*                   m_ResizeCallbackUserData;
    FWindowCloseCallback    m_CloseCallback;
    void*                   m_CloseCallbackUserData;
    FWindowFocusCallback    m_FocusCallback;
    void*                   m_FocusCallbackUserData;
    FWindowIconifyCallback  m_IconifyCallback;
    void*                   m_IconifyCallbackUserData;

    const char*             m_Title;
    uint32_t                m_Width;
    uint32_t                m_Height;
    uint32_t                m_Samples;
    uint32_t                m_BackgroundColor;
    uint8_t                 m_ContextAlphabits;
    uint8_t                 m_OpenGLVersionHint         : 7;
    uint8_t                 m_OpenGLUseCoreProfileHint  : 1;
    uint8_t                 m_Hidden                    : 1;
    uint8_t                 m_Fullscreen                : 1;
    uint8_t                 m_PrintDeviceInfo           : 1;
    uint8_t                 m_HighDPI                   : 1;
    uint8_t                                             : 4;
} WindowCreateParams;

/*# initialize window parameters
 * Initializes a WindowCreateParams struct with default values.
 * The struct is first cleared to zero, then `m_Width` is set to 640,
 * `m_Height` is set to 480, `m_Samples` is set to 1, and
 * `m_Title` is set to "Defold Application".
 * @name WindowCreateParamsInitialize
 * @param params [type:WindowCreateParams*] the params struct
 */
void WindowCreateParamsInitialize(WindowCreateParams* params);

/*# create a new window handle
 * @name WindowNew
 * @return window [type:HWindow] window handle
 */
HWindow WindowNew();

/*# delete a window handle
 * @name WindowDelete
 * @param window [type:HWindow] window handle
 */
void WindowDelete(HWindow window);

/*# open a window
 * @name WindowOpen
 * @param window [type:HWindow] window handle
 * @param params [type:WindowCreateParams*] window parameters
 * @return result [type:WindowResult] result code
 */
WindowResult WindowOpen(HWindow window, const WindowCreateParams* params);

/*# close the window
 * @name WindowClose
 * @param window [type:HWindow] window handle
 */
void WindowClose(HWindow window);

/*# show the window
 * @name WindowShow
 * @param window [type:HWindow] window handle
 */
void WindowShow(HWindow window);

/*# iconify the window
 * @name WindowIconify
 * @param window [type:HWindow] window handle
 */
void WindowIconify(HWindow window);

/*# get window width
 * @name WindowGetWidth
 * @param window [type:HWindow] window handle
 * @return width [type:uint32_t] window width
 */
uint32_t WindowGetWidth(HWindow window);

/*# get window height
 * @name WindowGetHeight
 * @param window [type:HWindow] window handle
 * @return height [type:uint32_t] window height
 */
uint32_t WindowGetHeight(HWindow window);

/*# get window state parameter
 * @name WindowGetStateParam
 * @param window [type:HWindow] window handle
 * @param state [type:WindowState] state parameter
 * @return value [type:uint32_t] parameter value
 */
uint32_t WindowGetStateParam(HWindow window, WindowState state);

/*# get display scale factor
 * @name WindowGetDisplayScaleFactor
 * @param window [type:HWindow] window handle
 * @return scale_factor [type:float] display scale factor
 */
float WindowGetDisplayScaleFactor(HWindow window);

/*# set window size
 * @name WindowSetSize
 * @param window [type:HWindow] window handle
 * @param width [type:uint32_t] window width
 * @param height [type:uint32_t] window height
 */
void WindowSetSize(HWindow window, uint32_t width, uint32_t height);

/*# poll window events
 * @name WindowPollEvents
 * @param window [type:HWindow] window handle
 */
void WindowPollEvents(HWindow window);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DMSDK_PLATFORM_WINDOW_H
