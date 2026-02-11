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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*# SDK Platform Window API documentation
 *
 * Window creation and event polling.
 *
 * @document
 * @name Platform Window
 * @namespace dmPlatform
 * @language C
 */

/*# Window handle
 * @typedef
 * @name HWindow
 */
typedef struct dmWindow* HWindow;

/*# window resize callback
 * @typedef
 * @name FWindowResizeCallback
 */
typedef void (*FWindowResizeCallback)(void* user_data, uint32_t width, uint32_t height);

/*# window focus callback
 * @typedef
 * @name FWindowFocusCallback
 */
typedef void (*FWindowFocusCallback)(void* user_data, uint32_t focus);

/*# window iconify callback
 * @typedef
 * @name FWindowIconifyCallback
 */
typedef void (*FWindowIconifyCallback)(void* user_data, uint32_t iconified);

/*# window close callback
 * @typedef
 * @name FWindowCloseCallback
 */
typedef bool (*FWindowCloseCallback)(void* user_data);

/*# keyboard char callback
 * @typedef
 * @name FWindowAddKeyboardCharCallback
 */
typedef void (*FWindowAddKeyboardCharCallback)(void* user_data, int chr);

/*# marked text callback
 * @typedef
 * @name FWindowSetMarkedTextCallback
 */
typedef void (*FWindowSetMarkedTextCallback)(void* user_data, char* text);

/*# device changed callback
 * @typedef
 * @name FWindowDeviceChangedCallback
 */
typedef void (*FWindowDeviceChangedCallback)(void* user_data, int status);

/*# result enumeration
 * @enum
 * @name WindowResult
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

    uint32_t                m_Width;
    uint32_t                m_Height;
    uint32_t                m_Samples;
    const char*             m_Title;
    bool                    m_Fullscreen;
    bool                    m_PrintDeviceInfo;
    bool                    m_HighDPI;
    uint32_t                m_BackgroundColor;
    uint8_t                 m_ContextAlphabits;
    uint8_t                 m_OpenGLVersionHint        : 7;
    uint8_t                 m_OpenGLUseCoreProfileHint : 1;
    uint8_t                 m_Hidden                   : 1;
    uint8_t                                            : 7;
} WindowCreateParams;

/*# initialize window parameters
 * Initializes a WindowCreateParams struct with default values.
 * @name WindowCreateParamsInitialize
 * @param params [type:WindowCreateParams*] the params struct
 */
void WindowCreateParamsInitialize(WindowCreateParams* params);

/*# create a new window handle
 * @name WindowNew
 * @return window [type:HWindow] window handle
 */
HWindow WindowNew(void);

/*# open a window
 * @name WindowOpen
 * @param window [type:HWindow] window handle
 * @param params [type:WindowCreateParams*] window parameters
 * @return result [type:WindowResult] result code
 */
WindowResult WindowOpen(HWindow window, const WindowCreateParams* params);

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

/*# close the window
 * @name WindowClose
 * @param window [type:HWindow] window handle
 */
void WindowClose(HWindow window);

/*# delete a window handle
 * @name WindowDelete
 * @param window [type:HWindow] window handle
 */
void WindowDelete(HWindow window);

/*# poll window events
 * @name WindowPollEvents
 * @param window [type:HWindow] window handle
 */
void WindowPollEvents(HWindow window);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DMSDK_PLATFORM_WINDOW_H
