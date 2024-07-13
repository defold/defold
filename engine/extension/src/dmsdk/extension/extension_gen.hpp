// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/extension and cmd=/Users/mathiaswesterdahl/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mathiaswesterdahl/work/defold/engine/extension/sdk_gen.json

// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_EXTENSION_GEN_HPP
#define DMSDK_EXTENSION_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# SDK Extension API documentation
 *
 * Functions for creating and controlling engine native extension libraries.
 *
 * @document
 * @language C++
 * @name Extension
 * @namespace dmExtension
 * @path engine/extension/src/dmsdk/extension/extension_gen.hpp
 */

#include <dmsdk/resource/resource.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/configfile.h>

#include <dmsdk/extension/extension.h>

namespace dmExtension
{
    /*# result enumeration
     * Result enumeration.
     * @enum
     * @name Result
     * @language C++
     * @member  EXTENSION_RESULT_OK
     * @member  EXTENSION_RESULT_INIT_ERROR
     */
    enum Result {
        RESULT_OK = 0,
        RESULT_INIT_ERROR = -1,
    };

    /*# event id enumeration
     * Event id enumeration.
     * 
     * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
     * @enum
     * @name EventID
     * @language C++
     * @member  EXTENSION_EVENT_ID_ACTIVATEAPP
     * @member  EXTENSION_EVENT_ID_DEACTIVATEAPP
     * @member  EXTENSION_EVENT_ID_ICONIFYAPP
     * @member  EXTENSION_EVENT_ID_DEICONIFYAPP
     * @member  EXTENSION_EVENT_ID_ENGINE_INITIALIZED
     * @member  EXTENSION_EVENT_ID_ENGINE_DELETE
     */
    enum EventID {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
        EVENT_ID_ICONIFYAPP,
        EVENT_ID_DEICONIFYAPP,
        EVENT_ID_ENGINE_INITIALIZED,
        EVENT_ID_ENGINE_DELETE,
    };

    /*# extra callback type
     * Extra callback type for RegisterCallback function.
     * @enum
     * @name CallbackType
     * @language C++
     * @member  EXTENSION_CALLBACK_PRE_RENDER
     * @member  EXTENSION_CALLBACK_POST_RENDER
     */
    enum CallbackType {
        CALLBACK_PRE_RENDER,
        CALLBACK_POST_RENDER,
    };

    /*#
        * Generated from [ref:ExtensionAppParams]
        */
    typedef ExtensionAppParams AppParams;

    /*# 
     * The global parameters avalable when registering and unregistering an extensioin
     * @struct
     * @name Params
     * @language C++
     * @member m_ConfigFile [type:HConfigFile] The game project settings (including overrides and plugins)
     * @member m_ResourceFactory [type:HResourceFactory] The game resource factory / repository
     * @member m_L [type:lua_State*] The Lua state.
     */
    typedef ExtensionParams Params;

    /*#
        * Generated from [ref:ExtensionEvent]
        */
    typedef ExtensionEvent Event;

    /*# Extra extension callback typedef
     * Callback typedef for functions passed to RegisterCallback().
     * @typedef
     * @name FCallback
     * @language C++
     * @param params [type:ExtensionParams] 
     * @return  [type:ExtensionResult] 
     */
    typedef FExtensionCallback FCallback;

    /*#
        * Generated from [ref:ExtensionRegisterCallback]
        */
    bool RegisterCallback(CallbackType callback_type, FCallback func);

    /*# Register application delegate
     * Register an iOS application delegate to the engine. Multiple delegates are supported (Max 32)
     * @name RegisteriOSUIApplicationDelegate
     * @language C++
     * @param delegate [type:id<UIApplicationDelegate>] An UIApplicationDelegate, see: https://developer.apple.com/documentation/uikit/uiapplicationdelegate?language=objc
     * @note Note that the delegate needs to be registered before the UIApplicationMain in order to
     * handle any earlier callbacks.
     * 
     * This function is only available on iOS. [icon:ios]
     * @examples 
     * ```objective-c
     * 
     * // myextension_ios.mm
     * 
     * id<UIApplicationDelegate> g_MyApplicationDelegate;
     * 
     * @interface MyApplicationDelegate : NSObject <UIApplicationDelegate>
     * 
     * - (void) applicationDidBecomeActive:(UIApplication *) application;
     * 
     * @end
     * 
     * @implementation MyApplicationDelegate
     * 
     * - (void) applicationDidBecomeActive:(UIApplication *) application {
     *     dmLogWarning("applicationDidBecomeActive - MyAppDelegate");
     * }
     * 
     * @end
     * 
     * struct MyAppDelegateRegister
     * {
     *     MyApplicationDelegate* m_Delegate;
     *     MyAppDelegateRegister() {
     *         m_Delegate = [[FacebookAppDelegate alloc] init];
     *         Extension::RegisteriOSUIApplicationDelegate(m_Delegate);
     *     }
     *     ~MyAppDelegateRegister() {
     *         Extension::UnregisteriOSUIApplicationDelegate(m_Delegate);
     *         [m_Delegate release];
     *     }
     * };
     * 
     * MyAppDelegateRegister g_FacebookDelegateRegister;
     * ```
     */
    void RegisteriOSUIApplicationDelegate(void * delegate);

    /*# Unregister an application delegate
     * Deregister a previously registered iOS application delegate
     * 
     * This function is only available on iOS. [icon:ios]
     * @name UnregisteriOSUIApplicationDelegate
     * @language C++
     * @param delegate [type:void*] an id<UIApplicationDelegate>
     */
    void UnregisteriOSUIApplicationDelegate(void * delegate);


} // namespace dmExtension

#endif // #define DMSDK_EXTENSION_GEN_HPP
/*# result enumeration
 * Result enumeration.
 * @enum
 * @name ExtensionResult
 * @language C
 * @member  EXTENSION_RESULT_OK
 * @member  EXTENSION_RESULT_INIT_ERROR
 */

/*# event id enumeration
 * Event id enumeration.
 * 
 * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
 * @enum
 * @name ExtensionEventID
 * @language C
 * @member  EXTENSION_EVENT_ID_ACTIVATEAPP
 * @member  EXTENSION_EVENT_ID_DEACTIVATEAPP
 * @member  EXTENSION_EVENT_ID_ICONIFYAPP
 * @member  EXTENSION_EVENT_ID_DEICONIFYAPP
 * @member  EXTENSION_EVENT_ID_ENGINE_INITIALIZED
 * @member  EXTENSION_EVENT_ID_ENGINE_DELETE
 */

/*# extra callback type
 * Extra callback type for RegisterCallback function.
 * @enum
 * @name ExtensionCallbackType
 * @language C
 * @member  EXTENSION_CALLBACK_PRE_RENDER
 * @member  EXTENSION_CALLBACK_POST_RENDER
 */

/*# 
 * The global parameters avalable when registering and unregistering an extensioin
 * @struct
 * @name ExtensionParams
 * @language C
 * @member m_ConfigFile [type:HConfigFile] The game project settings (including overrides and plugins)
 * @member m_ResourceFactory [type:HResourceFactory] The game resource factory / repository
 * @member m_L [type:lua_State*] The Lua state.
 */

/*# 
 * Callback when the app is being initialized. Called before [ref:FExtensionInitialize]
 * @typedef
 * @name FExtensionAppInitialize
 * @language C
 * @param params [type:ExtensionAppParams] 
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 * @note There is no guarantuee of initialization order. If an extension requires another extension to be initialized,
 *       that should be handled in [ref:FExtensionInitialize].
 */

/*# 
 * Callback when the app is being finalized
 * @typedef
 * @name FExtensionInitialize
 * @language C
 * @param params [type:ExtensionAppParams] 
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */

/*# 
 * Callback when the app is being finalized
 * @typedef
 * @name FExtensionInitialize
 * @language C
 * @param params [type:ExtensionAppParams] 
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */

/*# 
 * Calls for the finalization of an extension
 * @typedef
 * @name FExtensionFinalize
 * @language C
 * @param params [type:ExtensionParams] 
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 * @note All extensions will be called with `FExtensionFinalize` before moving on to the next step, the [ref:FExtensionAppFinalize]
 */

/*# 
 * Updates an extension. Called for each game frame.
 * @typedef
 * @name FExtensionUpdate
 * @language C
 * @param params [type:ExtensionParams] 
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */

/*# 
 * Receives an event from the engine
 * @typedef
 * @name FExtensionOnEvent
 * @language C
 * @param params [type:ExtensionParams] 
 * @param event [type:ExtensionEvent] The current event
 */

/*# Extra extension callback typedef
 * Callback typedef for functions passed to RegisterCallback().
 * @typedef
 * @name FExtensionCallback
 * @language C
 * @param params [type:ExtensionParams] 
 * @return  [type:ExtensionResult] 
 */

/*# Used when registering new extensions
 * Used when registering new extensions
 * @name ExtensionDescBufferSize
 * @language C
 */

/*# 
 * Extension declaration helper. Internal function. Use DM_DECLARE_EXTENSION
 * @name ExtensionRegister
 * @language C
 * @param desc [type:void*] A persistent buffer of at least 128 bytes.
 * @param desc_size [type:const char*] size of buffer holding desc. in bytes
 * @param name [type:const char*] extension name. human readble. max 16 characters long.
 * @param app_initialize [type:FExtensionAppInitialize] app-init function. May be null
 * @param app_finalize [type:FExtensionAppFinalize] app-final function. May be null
 * @param initialize [type:FExtensionInitialize] init function. May not be 0
 * @param finalize [type:FExtensionFinalize] finalize function. May not be 0
 * @param update [type:FExtensionUpdate] update function. May be null
 * @param on_event [type:FExtensionOnEvent] event callback function. May be null
 */

/*# Register application delegate
 * Register an iOS application delegate to the engine. Multiple delegates are supported (Max 32)
 * @name ExtensionRegisteriOSUIApplicationDelegate
 * @language C
 * @param delegate [type:id<UIApplicationDelegate>] An UIApplicationDelegate, see: https://developer.apple.com/documentation/uikit/uiapplicationdelegate?language=objc
 * @note Note that the delegate needs to be registered before the UIApplicationMain in order to
 * handle any earlier callbacks.
 * 
 * This function is only available on iOS. [icon:ios]
 * @examples 
 * ```objective-c
 * 
 * // myextension_ios.mm
 * 
 * id<UIApplicationDelegate> g_MyApplicationDelegate;
 * 
 * @interface MyApplicationDelegate : NSObject <UIApplicationDelegate>
 * 
 * - (void) applicationDidBecomeActive:(UIApplication *) application;
 * 
 * @end
 * 
 * @implementation MyApplicationDelegate
 * 
 * - (void) applicationDidBecomeActive:(UIApplication *) application {
 *     dmLogWarning("applicationDidBecomeActive - MyAppDelegate");
 * }
 * 
 * @end
 * 
 * struct MyAppDelegateRegister
 * {
 *     MyApplicationDelegate* m_Delegate;
 *     MyAppDelegateRegister() {
 *         m_Delegate = [[FacebookAppDelegate alloc] init];
 *         Extension::RegisteriOSUIApplicationDelegate(m_Delegate);
 *     }
 *     ~MyAppDelegateRegister() {
 *         Extension::UnregisteriOSUIApplicationDelegate(m_Delegate);
 *         [m_Delegate release];
 *     }
 * };
 * 
 * MyAppDelegateRegister g_FacebookDelegateRegister;
 * ```
 */

/*# Unregister an application delegate
 * Deregister a previously registered iOS application delegate
 * 
 * This function is only available on iOS. [icon:ios]
 * @name ExtensionUnregisteriOSUIApplicationDelegate
 * @language C
 * @param delegate [type:void*] an id<UIApplicationDelegate>
 */

/*# declare a new extension
 * Declare and register new extension to the engine.
 * This macro is used to declare the extension callback functions used by the engine to communicate with the extension.
 * @macro
 * @name DM_DECLARE_EXTENSION
 * @language C
 * @examples 
 * Register a new extension:
 * 
 * ```cpp
 * DM_DECLARE_EXTENSION(MyExt, "MyExt", AppInitializeMyExt, AppFinalizeMyExt, InitializeMyExt, UpdateMyExt, OnEventMyExt, FinalizeMyExt);
 * ```
 */

/*# Set if the platform is iPhoneOS [icon:ios]
 * Set if the platform is iPhoneOS [icon:ios]
 * @macro
 * @name DM_PLATFORM_IOS
 * @language C
 */

/*# Set if the platform is Android [icon:android]
 * Set if the platform is Android [icon:android]
 * @macro
 * @name DM_PLATFORM_ANDROID
 * @language C
 */

/*# Set if the platform is Html5 [icon:html5]
 * Set if the platform is Html5 [icon:html5]
 * @macro
 * @name DM_PLATFORM_HTML5
 * @language C
 */

/*# Set if the platform is OSX [icon:macos]
 * Set if the platform is OSX [icon:macos]
 * @macro
 * @name DM_PLATFORM_OSX
 * @language C
 */

/*# Set if the platform is Linux [icon:linux]
 * Set if the platform is Linux [icon:linux]
 * @macro
 * @name DM_PLATFORM_LINUX
 * @language C
 */

/*# Set if the platform is Windows [icon:windows] (on both x86 and x86_64)
 * Set if the platform is Windows [icon:windows] (on both x86 and x86_64)
 * @macro
 * @name DM_PLATFORM_WINDOWS
 * @language C
 */


