// Generated, do not edit!
// Generated from sdk_gen.json (C++ API).

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
 */

#include <dmsdk/dlib/deprecated.h>
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
     * @member  RESULT_OK
     * @member  RESULT_INIT_ERROR
     */
    typedef ::ExtensionResult Result;
    static const Result RESULT_OK = ::EXTENSION_RESULT_OK;
    static const Result RESULT_INIT_ERROR = ::EXTENSION_RESULT_INIT_ERROR;

    /*# event id enumeration
     * Event id enumeration.
     * 
     * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
     * @enum
     * @name EventID
     * @language C++
     * @member  EVENT_ID_ACTIVATEAPP
     * @member  EVENT_ID_DEACTIVATEAPP
     * @member  EVENT_ID_ICONIFYAPP
     * @member  EVENT_ID_DEICONIFYAPP
     * @member  EVENT_ID_ENGINE_INITIALIZED
     * @member  EVENT_ID_ENGINE_DELETE
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
     * @member  CALLBACK_PRE_RENDER
     * @member  CALLBACK_POST_RENDER
     */
    enum CallbackType {
        CALLBACK_PRE_RENDER,
        CALLBACK_POST_RENDER,
    };

    /*# engine exit code
     * Engine exit code.
     * @enum
     * @name AppExitCode
     * @language C++
     * @member  APP_EXIT_CODE_NONE
     * @member  APP_EXIT_CODE_REBOOT
     * @member  APP_EXIT_CODE_EXIT
     */
    enum AppExitCode {
        APP_EXIT_CODE_NONE = 0,
        APP_EXIT_CODE_REBOOT = 1,
        APP_EXIT_CODE_EXIT = -1,
    };

    /*# extension parameter enumeration
     * Parameters available through [ref:ParamsGetFloat].
     * @enum
     * @name Param
     * @language C++
     * @member  PARAM_DT engine update delta time
     */
    enum Param
    {
        PARAM_DT,
    };

    /*# 
     * The extension app parameters
     * @struct
     * @name AppParams
     * @language C++
     * @member m_ConfigFile [type:HConfigFile] Deprecated
     * @member m_ExitStatus [type:AppExitCode] App exit code
     */
    typedef ExtensionAppParams AppParams;

    /*# 
     * Parameters for an extension's engine lifecycle, update, render and event callbacks.
     * @struct
     * @name Params
     * @language C++
     * @member m_ConfigFile [type:HConfigFile] The game project settings (including overrides and plugins)
     * @member m_ResourceFactory [type:HResourceFactory] The game resource factory / repository
     * @member m_L [type:lua_State*] The Lua state.
     */
    typedef ExtensionParams Params;

    /*#
     * Extension event
     * @struct
     * @name Event
     * @language C++
     * @member m_Event [type:EventID]
     * @note Deprecated. Use FEventCallback with SetEventFn.
     */
    typedef ExtensionEvent Event;

    /*# Extra extension callback typedef
     * Callback typedef for functions passed to RegisterCallback().
     * @typedef
     * @name FCallback
     * @language C++
     * @param params [type:Params]
     * @return  [type:Result]
     */
    typedef FExtensionCallback FCallback;

    /*# extension handle
     * Opaque extension configuration handle. Borrowed during the register and
     * unregister callbacks; do not retain it. Configure callbacks only in register.
     * @typedef
     * @name HExtension
     * @language C++
     */
    typedef HExtension HExtension;

    /*# configure an extension
     * Called before any AppInitialize on each engine creation. Set the extension's
     * lifecycle callbacks through the Set*Fn functions. Callbacks not
     * set are optional. An update-only extension does not need an Initialize callback.
     * If registration fails, release any partially created state
     * before returning; unregister is called only after successful registration.
     * @typedef
     * @name FRegister
     * @language C++
     * @param extension [type:HExtension] borrowed extension configuration handle
     * @return result [type:Result] RESULT_OK on success
     */
    typedef FExtensionRegister FRegister;

    /*# unregister an extension
     * Called after all AppFinalize callbacks for each successfully registered extension,
     * including when a later initialization callback failed. The handle remains
     * valid for this call. Release registration-owned state here.
     * @typedef
     * @name FUnregister
     * @language C++
     * @param extension [type:HExtension] borrowed extension configuration handle
     * @return result [type:Result] RESULT_OK on success
     */
    typedef FExtensionUnregister FUnregister;

    /*# receive a generic engine event
     * Synchronous delivery on the engine thread, after successful AppInitialize (or
     * when it is omitted) until that extension's AppFinalize begins. Engine Initialize is not required:
     * window events and cleanup after a failed initialization may have partial contexts.
     * Event dispatch is not thread-safe. Publish events only on the engine thread.
     * Callbacks run synchronously on the calling thread; dispatch
     * does not marshal calls from worker threads. Nested dispatch on the same
     * engine thread is supported.
     * One catch-all callback receives every generic event. Parameters and data are
     * borrowed for this call only. Inspect event_id to choose which events to handle;
     * no per-event registration is required. Payload-free events pass null data and
     * data_size zero. Receivers interpret data according to the event's documented
     * payload type and size. Delivery order between extensions is unspecified.
     * All events are broadcasts; every eligible handler is called
     * and events cannot be consumed. Extensions can publish custom events through
     * BroadcastEvent. Ignore ids you do not recognize.
     *
     * Built-in engine event definitions are available in
     * `<dmsdk/engine/engine_event.h>`.
     * @typedef
     * @name FEventCallback
     * @language C++
     * @param params [type:Params*] callback parameters; available contexts depend on lifecycle phase
     * @param event_id [type:dmhash_t] event name hash
     * @param data [type:const void*] immutable borrowed payload, or null
     * @param data_size [type:uint32_t] payload size in bytes; zero for payload-free events
     */
    typedef FExtensionEventCallback FEventCallback;

    /*# 
     * Initializes an extension app params struct
     * NOTE: this is an opaque struct, do not use it's members directly!
     * @name AppParamsInitialize
     * @language C++
     * @param app_params [type:AppParams*] the params
     */
    void AppParamsInitialize(AppParams * app_params);

    /*# 
     * Finalizes an extension app params struct (deallocates internal memory)
     * @name AppParamsFinalize
     * @language C++
     * @param app_params [type:AppParams*] the params
     */
    void AppParamsFinalize(AppParams * params);

    /*# 
     * Initializes an extension params struct
     * NOTE: this is an opaque struct, do not use it's members directly!
     * @name ParamsInitialize
     * @language C++
     * @param app_params [type:Params*] the params
     */
    void ParamsInitialize(Params * app_params);

    /*# 
     * Finalizes an extension  params struct (deallocates internal memory)
     * @name ParamsFinalize
     * @language C++
     * @param app_params [type:Params*] the params
     */
    void ParamsFinalize(Params * params);

    /*#
     * Gets a float from the extension callback parameters.
     *
     * Supported parameters:
     *
     * | Parameter | Description |
     * | --------- | ----------- |
     * | `PARAM_DT` | Current engine update delta in seconds, before collection-proxy time scaling. Zero outside the extension update callback. |
     * @name ParamsGetFloat
     * @language C++
     * @param params [type:const Params*] valid parameters for the current callback
     * @param param [type:Param] parameter to read, for example `PARAM_DT`
     * @param out_value [type:float*] output value; left unchanged if the parameter is unsupported
     * @return result [type:bool] true if the parameter is supported, otherwise false
     */
    bool ParamsGetFloat(const Params* params, Param param, float* out_value);

    /*# 
     * Sets a context using a specified name
     * @name AppParamsSetContext
     * @language C++
     * @param params [type:AppParams] the params
     * @param name [type:const char*] the context name
     * @param context [type:void*] the context
     * @return result [type:int] 0 if successful
     * @note Deprecated. Use [ref:ContextRegistrySet] on [ref:HContextRegistry] instead.
     */
    int AppParamsSetContext(AppParams * params, const char * name, void * context);

    /*# 
     * Gets a context using a specified name
     * @name AppParamsGetContextByName
     * @language C++
     * @param params [type:AppParams] the params
     * @param name [type:const char*] the context name
     * @return context [type:void*] The context, if it exists
     * @note Deprecated. Use [ref:ContextRegistryGet] on [ref:HContextRegistry] instead.
     */
    void * AppParamsGetContextByName(AppParams * params, const char * name);

    /*# 
     * Gets a context using a specified name hash
     * @name AppParamsGetContext
     * @language C++
     * @param params [type:AppParams] the params
     * @param name_hash [type:dmhash_t] the context name hash
     * @return context [type:void*] The context, if it exists
     * @note Deprecated. Use [ref:ContextRegistryGetByHash] on [ref:HContextRegistry] instead.
     */
    void * AppParamsGetContext(AppParams * params, dmhash_t name_hash);

    /*# get the app exit code
     * get the app exit code
     * @name AppParamsGetAppExitCode
     * @language C++
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return code [type:AppExitCode] engine exit code
     */
    AppExitCode AppParamsGetAppExitCode(AppParams * app_params);

    /*# 
     * Sets a context using a specified name
     * @name ParamsSetContext
     * @language C++
     * @param params [type:Params] the params
     * @param name [type:const char*] the context name
     * @param context [type:void*] the context
     * @return result [type:int] 0 if successful
     * @note Deprecated. Use [ref:ContextRegistrySet] on [ref:HContextRegistry] instead.
     */
    int ParamsSetContext(Params * params, const char * name, void * context);

    /*# 
     * Gets a context using a specified name
     * @name ParamsGetContextByName
     * @language C++
     * @param params [type:Params] the params
     * @param name [type:const char*] the context name
     * @return context [type:void*] The context, if it exists
     * @note Deprecated. Use [ref:ContextRegistryGet] on [ref:HContextRegistry] instead.
     */
    void * ParamsGetContextByName(Params * params, const char * name);

    /*# 
     * Gets a context using a specified name hash
     * @name ParamsGetContext
     * @language C++
     * @param params [type:Params] the params
     * @param name_hash [type:dmhash_t] the context name hash
     * @return context [type:void*] The context, if it exists
     * @note Deprecated. Use [ref:ContextRegistryGetByHash] on [ref:HContextRegistry] instead.
     */
    void * ParamsGetContext(Params * params, dmhash_t name_hash);

    /*#
        * Generated from [ref:ExtensionRegisterCallback]
        * @name RegisterCallback
        */
    bool RegisterCallback(CallbackType callback_type, FCallback func);

    /*# get the extension's application user data
     * Returns the pointer associated with this extension by SetUserData.
     * Available during AppInitialize and AppFinalize, including failed initialization
     * cleanup. Returns null if no pointer was set or outside an extension callback.
     * The extension owns the memory; do not retain the borrowed parameters.
     * @name AppParamsGetUserData
     * @language C++
     * @param params [type:const AppParams*] valid application callback parameters
     * @return user_data [type:void*] this extension's user data, or null
     */
    void* AppParamsGetUserData(const AppParams* params);

    /*# get the extension's user data
     * Returns the pointer associated with this extension by SetUserData
     * during Initialize, Update, Finalize, pre/post-render and event callbacks.
     * Nested callback delivery restores the outer callback's pointer on return.
     * Returns null if no pointer was set or outside an extension callback.
     * The extension owns the memory; do not retain the borrowed parameters.
     * @name ParamsGetUserData
     * @language C++
     * @param params [type:const Params*] valid callback parameters
     * @return user_data [type:void*] this extension's user data, or null
     */
    void* ParamsGetUserData(const Params* params);

    /*# associate user data with an extension
     * Call only from the register callback. The engine borrows this pointer;
     * it does not allocate, copy or free the memory. Passing null clears it.
     * Retrieve it from callback parameters with ParamsGetUserData or
     * AppParamsGetUserData, and from the unregister callback with
     * GetUserData. Keep it valid through all callbacks, then release
     * it in unregister. If registration fails, release it before returning because
     * unregister is not called. The pointer resets to null before each registration
     * and after unregister, including engine reboot.
     * @name SetUserData
     * @language C++
     * @param extension [type:HExtension] borrowed extension configuration handle
     * @param user_data [type:void*] extension-owned pointer, or null
     */
    void SetUserData(HExtension extension, void* user_data);

    /*# get an extension's user data
     * Use during register or unregister to retrieve the pointer associated with
     * this extension by SetUserData. The extension owns the memory.
     * @name GetUserData
     * @language C++
     * @param extension [type:HExtension] borrowed extension configuration handle
     * @return user_data [type:void*] this extension's user data, or null
     */
    void* GetUserData(HExtension extension);

    /*# set the application initialization callback
     * Call only from the register callback. Passing null clears the callback.
     * @name SetAppInitializeFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FExtensionAppInitialize] callback, or null
     */
    void SetAppInitializeFn(HExtension extension, FExtensionAppInitialize fn);

    /*# set the application finalization callback
     * Call only from the register callback. Passing null clears the callback.
     * @name SetAppFinalizeFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FExtensionAppFinalize] callback, or null
     */
    void SetAppFinalizeFn(HExtension extension, FExtensionAppFinalize fn);

    /*# set the engine initialization callback
     * Call only from the register callback. Passing null clears the callback.
     * @name SetInitializeFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FExtensionInitialize] callback, or null
     */
    void SetInitializeFn(HExtension extension, FExtensionInitialize fn);

    /*# set the engine finalization callback
     * Call only from the register callback. Passing null clears the callback.
     * @name SetFinalizeFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FExtensionFinalize] callback, or null
     */
    void SetFinalizeFn(HExtension extension, FExtensionFinalize fn);

    /*# set the engine update callback
     * Call only from the register callback. Passing null clears the callback.
     * Updates begin after successful engine initialization and stop before Finalize,
     * even when no Initialize or Finalize callback is configured.
     * @name SetUpdateFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FExtensionUpdate] callback, or null
     */
    void SetUpdateFn(HExtension extension, FExtensionUpdate fn);

    /*# set the pre-render callback
     * Call only from the register callback. Passing null clears the callback.
     * Delivery requires successful AppInitialize, or no AppInitialize callback, and
     * stops before AppFinalize. It does not require an Initialize callback.
     * @name SetPreRenderFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FCallback] callback, or null
     */
    void SetPreRenderFn(HExtension extension, FCallback fn);

    /*# set the post-render callback
     * Call only from the register callback. Passing null clears the callback.
     * Delivery requires successful AppInitialize, or no AppInitialize callback, and
     * stops before AppFinalize. It does not require an Initialize callback.
     * @name SetPostRenderFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FCallback] callback, or null
     */
    void SetPostRenderFn(HExtension extension, FCallback fn);

    /*# set the generic event callback
     * Call only from the register callback. One handler receives all generic
     * events; use the callback's event_id to select events. No event-name argument
     * or per-event registration is needed. Passing null clears it.
     * Registration resets on engine reboot; no event is replayed to a late receiver.
     * @name SetEventFn
     * @language C++
     * @param extension [type:HExtension] extension configuration handle
     * @param fn [type:FEventCallback] callback, or null
     */
    void SetEventFn(HExtension extension, FEventCallback fn);

    /*# broadcast an extension event
     * Synchronously delivers an event to every eligible generic extension handler.
     * The engine accepts arbitrary event hashes and opaque payloads without event
     * or payload registration. Receivers ignore event ids they do not recognize.
     * Use extension-specific names for custom events; names beginning with `_dm_event_`
     * are reserved for engine events.
     *
     * This function is not thread-safe. Call only on the engine thread with the
     * parameters received from an engine callback. Callbacks run on the calling
     * thread; no queuing or worker-thread marshalling occurs. Nested dispatch is
     * supported. Delivery order is unspecified; events cannot be consumed.
     * Parameters and payload are borrowed until return.
     * The sender retains ownership. Data is read-only to receivers. Passing null
     * data with data_size zero sends a payload-free event. The engine forwards
     * the payload without interpreting its contents.
     * @name BroadcastEvent
     * @language C++
     * @param params [type:Params*] valid current engine callback parameters
     * @param event_id [type:dmhash_t] hash of the event name
     * @param data [type:const void*] borrowed payload, or null for a payload-free event
     * @param data_size [type:uint32_t] payload size in bytes; zero for a payload-free event
     * @examples
     * ```c
     * uint32_t value = 42;
     * BroadcastEvent(params, dmHashString64("my_extension.changed"), &value, sizeof(value));
     * ```
     */
    void BroadcastEvent(Params* params, dmhash_t event_id, const void* data, uint32_t data_size);

    /*# Register application delegate
     * Register an iOS application delegate for process-level callbacks such as launch
     * and remote notification registration. Multiple delegates are supported (Max 32).
     * Use application:willFinishLaunchingWithOptions: and
     * application:didFinishLaunchingWithOptions: for launch. The older
     * applicationDidFinishLaunching: callback is not synthesized.
     * Defold uses the UIScene lifecycle. Application activity, URL and user activity
     * callbacks are not forwarded from scenes; use [ref:RegisteriOSUISceneDelegate].
     * The game window is created after application launch. Use scene connection to
     * access it, and scene connection options for cold-launch URLs and user activities.
     *
     * application:supportedInterfaceOrientationsForWindow: can override the game's
     * orientation settings, including on iOS 27 and later. The last nonzero mask
     * containing only valid UIInterfaceOrientationMask bits in delegate dispatch
     * order is used. Invalid masks are ignored. The application, scene, and root
     * view controller share this policy, including during full-screen presentations.
     *
     * The game's default mask comes from the DefoldSupportedInterfaceOrientations
     * Info.plist array, with DefoldSupportedInterfaceOrientations~ipad taking
     * precedence on iPad. If neither contains usable orientations, Defold falls back
     * to UISupportedInterfaceOrientations~ipad on iPad, then
     * UISupportedInterfaceOrientations. Arrays contain UIKit orientation names such
     * as UIInterfaceOrientationPortrait. The default policy excludes upside-down
     * portrait on iPhone. If no array is usable, all device-supported orientations
     * are allowed. The built-in manifest keeps the UIKit arrays broad and uses the
     * Defold array for the game's orientation settings.
     *
     * UIKit may ignore orientation restrictions, including on iPhone Duo's inner
     * display. Games must still handle window size changes.
     *
     * Register before scene connection to affect the initial orientation.
     * To update the policy after scene connection, call
     * setNeedsUpdateOfSupportedInterfaceOrientations on the root view controller
     * (iOS 16+) or attemptRotationToDeviceOrientation (iOS 15).
     * @name RegisteriOSUIApplicationDelegate
     * @language C++
     * @param delegate [type:void*] An id<UIApplicationDelegate>.
     * @note Register before UIApplicationMain to receive launch callbacks. Registration
     * does not retain the delegate; keep it alive until it is unregistered.
     * This function is only available on iOS. [icon:ios]
     */
    void RegisteriOSUIApplicationDelegate(void * delegate);

    /*# Unregister an application delegate
     * Deregister a previously registered iOS application delegate.
     * This function is only available on iOS. [icon:ios]
     * @name UnregisteriOSUIApplicationDelegate
     * @language C++
     * @param delegate [type:void*] An id<UIApplicationDelegate>.
     */
    void UnregisteriOSUIApplicationDelegate(void * delegate);

    /*# Register a scene delegate
     * Register an iOS scene observer. Multiple delegates are supported (Max 32).
     * Defold owns the single game window and its scene configuration. Registered
     * observers receive scene connection/disconnection, activation/deactivation,
     * foreground/background, openURLContexts, continueUserActivity,
     * willContinueUserActivityWithType, didFailToContinueUserActivityWithType:error:
     * and didUpdateUserActivity callbacks. Other optional delegate methods are not
     * forwarded. No legacy UIApplicationDelegate callbacks are synthesized.
     *
     * Register before UIApplicationMain (for example in a static constructor) to
     * receive the initial scene connection. Extension app-initialize is too late.
     * Cold-launch URLs, user activities and notification responses are available in
     * UISceneConnectionOptions. Scene connection may precede Lua initialization;
     * extensions must retain any data they need to deliver to Lua later.
     * @name RegisteriOSUISceneDelegate
     * @language C++
     * @param delegate [type:void*] An id<UISceneDelegate>.
     * @note Register and unregister on the main thread. Registration does not retain
     * the delegate; keep it alive until unregistering. Each event retains a snapshot
     * of its observers, so unregistering during delivery affects subsequent events.
     * Duplicate registrations are ignored. Late registration does not replay events.
     * This function is only available on iOS. [icon:ios]
     * @examples
     * ```objective-c
     * // myextension_ios.mm
     * @interface MySceneDelegate : NSObject <UISceneDelegate>
     * - (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options;
     * - (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts;
     * @end
     *
     * struct MySceneDelegateRegistration
     * {
     *     MySceneDelegate* m_Delegate;
     *     MySceneDelegateRegistration()
     *     {
     *         m_Delegate = [[MySceneDelegate alloc] init];
     *         dmExtension::RegisteriOSUISceneDelegate(m_Delegate);
     *     }
     *     ~MySceneDelegateRegistration()
     *     {
     *         dmExtension::UnregisteriOSUISceneDelegate(m_Delegate);
     *         [m_Delegate release];
     *     }
     * };
     * MySceneDelegateRegistration g_MySceneDelegateRegistration;
     * ```
     */
    void RegisteriOSUISceneDelegate(void * delegate);

    /*# Unregister a scene delegate
     * Deregister a previously registered iOS scene observer on the main thread.
     * This function is only available on iOS. [icon:ios]
     * @name UnregisteriOSUISceneDelegate
     * @language C++
     * @param delegate [type:void*] An id<UISceneDelegate>.
     */
    void UnregisteriOSUISceneDelegate(void * delegate);


} // namespace dmExtension

/*# extension handle
 * Opaque extension configuration handle. Borrowed during the register and
 * unregister callbacks; do not retain it. Configure callbacks only in register.
 * @typedef
 * @name HExtension
 * @language C
 */

/*# configure an extension
 * Called before any AppInitialize on each engine creation. Set the extension's
 * lifecycle callbacks through the ExtensionSet*Fn functions. Callbacks not
 * set are optional. An update-only extension does not need an Initialize callback.
 * If registration fails, release any partially created state
 * before returning; unregister is called only after successful registration.
 * @typedef
 * @name FExtensionRegister
 * @language C
 * @param extension [type:HExtension] borrowed extension configuration handle
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK on success
 */

/*# unregister an extension
 * Called after all AppFinalize callbacks for each successfully registered extension,
 * including when a later initialization callback failed. The handle remains
 * valid for this call. Release registration-owned state here.
 * @typedef
 * @name FExtensionUnregister
 * @language C
 * @param extension [type:HExtension] borrowed extension configuration handle
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK on success
 */

/*# set the application initialization callback
 * Call only from the register callback. Passing null clears the callback.
 * @name ExtensionSetAppInitializeFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionAppInitialize] callback, or null
 */

/*# set the application finalization callback
 * Call only from the register callback. Passing null clears the callback.
 * @name ExtensionSetAppFinalizeFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionAppFinalize] callback, or null
 */

/*# set the engine initialization callback
 * Call only from the register callback. Passing null clears the callback.
 * @name ExtensionSetInitializeFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionInitialize] callback, or null
 */

/*# set the engine finalization callback
 * Call only from the register callback. Passing null clears the callback.
 * @name ExtensionSetFinalizeFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionFinalize] callback, or null
 */

/*# set the engine update callback
 * Call only from the register callback. Passing null clears the callback.
 * Updates begin after successful engine initialization and stop before Finalize,
 * even when no Initialize or Finalize callback is configured.
 * @name ExtensionSetUpdateFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionUpdate] callback, or null
 */

/*# set the pre-render callback
 * Call only from the register callback. Passing null clears the callback.
 * Delivery requires successful AppInitialize, or no AppInitialize callback, and
 * stops before AppFinalize. It does not require an Initialize callback.
 * @name ExtensionSetPreRenderFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionCallback] callback, or null
 */

/*# set the post-render callback
 * Call only from the register callback. Passing null clears the callback.
 * Delivery requires successful AppInitialize, or no AppInitialize callback, and
 * stops before AppFinalize. It does not require an Initialize callback.
 * @name ExtensionSetPostRenderFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionCallback] callback, or null
 */

/*# extension declaration helper
 * Internal function used by the three-argument DM_DECLARE_EXTENSION macro.
 * The descriptor storage and name must remain valid for the process lifetime.
 * @name ExtensionRegisterDesc
 * @language C
 * @param desc [type:void*] aligned descriptor buffer
 * @param desc_size [type:uint32_t] descriptor buffer size in bytes
 * @param name [type:const char*] extension name
 * @param register_fn [type:FExtensionRegister] configuration callback
 * @param unregister_fn [type:FExtensionUnregister] cleanup callback, or null
 */

/*# receive a generic engine event
 * Synchronous delivery on the engine thread, after successful AppInitialize (or
 * when it is omitted) until that extension's AppFinalize begins. Engine Initialize is not required:
 * window events and cleanup after a failed initialization may have partial contexts.
 * Event dispatch is not thread-safe. Publish events only on the engine thread.
 * Callbacks run synchronously on the calling thread; dispatch
 * does not marshal calls from worker threads. Nested dispatch on the same
 * engine thread is supported.
 * One catch-all callback receives every generic event. Parameters and data are
 * borrowed for this call only. Inspect event_id to choose which events to handle;
 * no per-event registration is required. Payload-free events pass null data and
 * data_size zero. Receivers interpret data according to the event's documented
 * payload type and size. Delivery order between extensions is unspecified.
 * All events are broadcasts; every eligible handler is called
 * and events cannot be consumed. Extensions can publish custom events through
 * ExtensionBroadcastEvent. Ignore ids you do not recognize.
 *
 * Built-in engine event definitions are available in
 * `<dmsdk/engine/engine_event.h>`.
 * @typedef
 * @name FExtensionEventCallback
 * @language C
 * @param params [type:ExtensionParams*] callback parameters; available contexts depend on lifecycle phase
 * @param event_id [type:dmhash_t] event name hash
 * @param data [type:const void*] immutable borrowed payload, or null
 * @param data_size [type:uint32_t] payload size in bytes; zero for payload-free events
 */

/*# set the generic event callback
 * Call only from the register callback. One handler receives all generic
 * events; use the callback's event_id to select events. No event-name argument
 * or per-event registration is needed. Passing null clears it.
 * Registration resets on engine reboot; no event is replayed to a late receiver.
 * @name ExtensionSetEventFn
 * @language C
 * @param extension [type:HExtension] extension configuration handle
 * @param fn [type:FExtensionEventCallback] callback, or null
 */

/*# get the extension's application user data
 * Returns the pointer associated with this extension by ExtensionSetUserData.
 * Available during AppInitialize and AppFinalize, including failed initialization
 * cleanup. Returns null if no pointer was set or outside an extension callback.
 * The extension owns the memory; do not retain the borrowed parameters.
 * @name ExtensionAppParamsGetUserData
 * @language C
 * @param params [type:const ExtensionAppParams*] valid application callback parameters
 * @return user_data [type:void*] this extension's user data, or null
 */

/*# get the extension's user data
 * Returns the pointer associated with this extension by ExtensionSetUserData
 * during Initialize, Update, Finalize, pre/post-render and event callbacks.
 * Nested callback delivery restores the outer callback's pointer on return.
 * Returns null if no pointer was set or outside an extension callback.
 * The extension owns the memory; do not retain the borrowed parameters.
 * @name ExtensionParamsGetUserData
 * @language C
 * @param params [type:const ExtensionParams*] valid callback parameters
 * @return user_data [type:void*] this extension's user data, or null
 */

/*# associate user data with an extension
 * Call only from the register callback. The engine borrows this pointer;
 * it does not allocate, copy or free the memory. Passing null clears it.
 * Retrieve it from callback parameters with ExtensionParamsGetUserData or
 * ExtensionAppParamsGetUserData, and from the unregister callback with
 * ExtensionGetUserData. Keep it valid through all callbacks, then release
 * it in unregister. If registration fails, release it before returning because
 * unregister is not called. The pointer resets to null before each registration
 * and after unregister, including engine reboot.
 * @name ExtensionSetUserData
 * @language C
 * @param extension [type:HExtension] borrowed extension configuration handle
 * @param user_data [type:void*] extension-owned pointer, or null
 */

/*# get an extension's user data
 * Use during register or unregister to retrieve the pointer associated with
 * this extension by ExtensionSetUserData. The extension owns the memory.
 * @name ExtensionGetUserData
 * @language C
 * @param extension [type:HExtension] borrowed extension configuration handle
 * @return user_data [type:void*] this extension's user data, or null
 */

/*# broadcast an extension event
 * Synchronously delivers an event to every eligible generic extension handler.
 * The engine accepts arbitrary event hashes and opaque payloads without event
 * or payload registration. Receivers ignore event ids they do not recognize.
 * Use extension-specific names for custom events; names beginning with `_dm_event_`
 * are reserved for engine events.
 *
 * This function is not thread-safe. Call only on the engine thread with the
 * parameters received from an engine callback. Callbacks run on the calling
 * thread; no queuing or worker-thread marshalling occurs. Nested dispatch is
 * supported. Delivery order is unspecified; events cannot be consumed.
 * Parameters and payload are borrowed until return.
 * The sender retains ownership. Data is read-only to receivers. Passing null
 * data with data_size zero sends a payload-free event. The engine forwards
 * the payload without interpreting its contents.
 * @name ExtensionBroadcastEvent
 * @language C
 * @param params [type:ExtensionParams*] valid current engine callback parameters
 * @param event_id [type:dmhash_t] hash of the event name
 * @param data [type:const void*] borrowed payload, or null for a payload-free event
 * @param data_size [type:uint32_t] payload size in bytes; zero for a payload-free event
 * @examples
 * ```c
 * uint32_t value = 42;
 * ExtensionBroadcastEvent(params, dmHashString64("my_extension.changed"), &value, sizeof(value));
 * ```
 */

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

/*# engine exit code
 * Engine exit code.
 * @enum
 * @name ExtensionAppExitCode
 * @language C
 * @member  EXTENSION_APP_EXIT_CODE_NONE
 * @member  EXTENSION_APP_EXIT_CODE_REBOOT
 * @member  EXTENSION_APP_EXIT_CODE_EXIT
 */

/*# 
 * The extension app parameters
 * @struct
 * @name ExtensionAppParams
 * @language C
 * @member m_ConfigFile [type:HConfigFile] Deprecated
 * @member m_ExitStatus [type:ExtensionAppExitCode] App exit code
 */

/*# 
 * Parameters for an extension's engine lifecycle, update, render and event callbacks.
 * @struct
 * @name ExtensionParams
 * @language C
 * @member m_ConfigFile [type:HConfigFile] The game project settings (including overrides and plugins)
 * @member m_ResourceFactory [type:HResourceFactory] The game resource factory / repository
 * @member m_L [type:lua_State*] The Lua state.
 */

/*#
 * Extension event
 * @struct
 * @name ExtensionEvent
 * @language C
 * @member m_Event [type:ExtensionEventID]
 * @note Deprecated. Use FExtensionEventCallback with ExtensionSetEventFn.
 */

/*# 
 * Initializes an extension app params struct
 * NOTE: this is an opaque struct, do not use it's members directly!
 * @name ExtensionAppParamsInitialize
 * @language C
 * @param app_params [type:ExtensionAppParams*] the params
 */

/*# 
 * Finalizes an extension app params struct (deallocates internal memory)
 * @name ExtensionAppParamsFinalize
 * @language C
 * @param app_params [type:ExtensionAppParams*] the params
 */

/*# 
 * Initializes an extension params struct
 * NOTE: this is an opaque struct, do not use it's members directly!
 * @name ExtensionParamsInitialize
 * @language C
 * @param app_params [type:ExtensionParams*] the params
 */

/*# 
 * Finalizes an extension  params struct (deallocates internal memory)
 * @name ExtensionParamsFinalize
 * @language C
 * @param app_params [type:ExtensionParams*] the params
 */

/*# extension parameter enumeration
 * Parameters available through [ref:ExtensionParamsGetFloat].
 * @enum
 * @name ExtensionParam
 * @language C
 * @member  EXTENSION_PARAM_DT engine update delta time
 */

/*#
 * Gets a float from the extension callback parameters.
 *
 * Supported parameters:
 *
 * | Parameter | Description |
 * | --------- | ----------- |
 * | `EXTENSION_PARAM_DT` | Current engine update delta in seconds, before collection-proxy time scaling. Zero outside the extension update callback. |
 * @name ExtensionParamsGetFloat
 * @language C
 * @param params [type:const ExtensionParams*] valid parameters for the current callback
 * @param param [type:ExtensionParam] parameter to read, for example `EXTENSION_PARAM_DT`
 * @param out_value [type:float*] output value; left unchanged if the parameter is unsupported
 * @return result [type:bool] true if the parameter is supported, otherwise false
 */

/*# 
 * Sets a context using a specified name
 * @name ExtensionAppParamsSetContext
 * @language C
 * @param params [type:ExtensionAppParams] the params
 * @param name [type:const char*] the context name
 * @param context [type:void*] the context
 * @return result [type:int] 0 if successful
 * @note Deprecated. Use [ref:ContextRegistrySet] on [ref:HContextRegistry] instead.
 */

/*# 
 * Gets a context using a specified name
 * @name ExtensionAppParamsGetContextByName
 * @language C
 * @param params [type:ExtensionAppParams] the params
 * @param name [type:const char*] the context name
 * @return context [type:void*] The context, if it exists
 * @note Deprecated. Use [ref:ContextRegistryGet] on [ref:HContextRegistry] instead.
 */

/*# 
 * Gets a context using a specified name hash
 * @name ExtensionAppParamsGetContext
 * @language C
 * @param params [type:ExtensionAppParams] the params
 * @param name_hash [type:dmhash_t] the context name hash
 * @return context [type:void*] The context, if it exists
 * @note Deprecated. Use [ref:ContextRegistryGetByHash] on [ref:HContextRegistry] instead.
 */

/*# get the app exit code
 * get the app exit code
 * @name ExtensionAppParamsGetAppExitCode
 * @language C
 * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
 * @return code [type:ExtensionAppExitCode] engine exit code
 */

/*# 
 * Sets a context using a specified name
 * @name ExtensionParamsSetContext
 * @language C
 * @param params [type:ExtensionParams] the params
 * @param name [type:const char*] the context name
 * @param context [type:void*] the context
 * @return result [type:int] 0 if successful
 * @note Deprecated. Use [ref:ContextRegistrySet] on [ref:HContextRegistry] instead.
 */

/*# 
 * Gets a context using a specified name
 * @name ExtensionParamsGetContextByName
 * @language C
 * @param params [type:ExtensionParams] the params
 * @param name [type:const char*] the context name
 * @return context [type:void*] The context, if it exists
 * @note Deprecated. Use [ref:ContextRegistryGet] on [ref:HContextRegistry] instead.
 */

/*# 
 * Gets a context using a specified name hash
 * @name ExtensionParamsGetContext
 * @language C
 * @param params [type:ExtensionParams] the params
 * @param name_hash [type:dmhash_t] the context name hash
 * @return context [type:void*] The context, if it exists
 * @note Deprecated. Use [ref:ContextRegistryGetByHash] on [ref:HContextRegistry] instead.
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
 * Callback after all extensions have been called with [ref:FExtensionAppInitialize]
 * @typedef
 * @name FExtensionInitialize
 * @language C
 * @param params [type:ExtensionParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */

/*# 
 * Callback when the app is being finalized
 * @typedef
 * @name FExtensionAppFinalize
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
 * @note Deprecated. Use FExtensionEventCallback with ExtensionSetEventFn. A warning is logged once per registered legacy handler.
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
 * @constant
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
 * @note Deprecated. Use the three-argument declaration and setters instead.
 */

/*# Register application delegate
 * Register an iOS application delegate for process-level callbacks such as launch
 * and remote notification registration. Multiple delegates are supported (Max 32).
 * Use application:willFinishLaunchingWithOptions: and
 * application:didFinishLaunchingWithOptions: for launch. The older
 * applicationDidFinishLaunching: callback is not synthesized.
 * Defold uses the UIScene lifecycle. Application activity, URL and user activity
 * callbacks are not forwarded from scenes; use [ref:ExtensionRegisteriOSUISceneDelegate].
 * The game window is created after application launch. Use scene connection to
 * access it, and scene connection options for cold-launch URLs and user activities.
 *
 * application:supportedInterfaceOrientationsForWindow: can override the game's
 * orientation settings, including on iOS 27 and later. The last nonzero mask
 * containing only valid UIInterfaceOrientationMask bits in delegate dispatch
 * order is used. Invalid masks are ignored. The application, scene, and root
 * view controller share this policy, including during full-screen presentations.
 *
 * The game's default mask comes from the DefoldSupportedInterfaceOrientations
 * Info.plist array, with DefoldSupportedInterfaceOrientations~ipad taking
 * precedence on iPad. If neither contains usable orientations, Defold falls back
 * to UISupportedInterfaceOrientations~ipad on iPad, then
 * UISupportedInterfaceOrientations. Arrays contain UIKit orientation names such
 * as UIInterfaceOrientationPortrait. The default policy excludes upside-down
 * portrait on iPhone. If no array is usable, all device-supported orientations
 * are allowed. The built-in manifest keeps the UIKit arrays broad and uses the
 * Defold array for the game's orientation settings.
 *
 * UIKit may ignore orientation restrictions, including on iPhone Duo's inner
 * display. Games must still handle window size changes.
 *
 * Register before scene connection to affect the initial orientation.
 * To update the policy after scene connection, call
 * setNeedsUpdateOfSupportedInterfaceOrientations on the root view controller
 * (iOS 16+) or attemptRotationToDeviceOrientation (iOS 15).
 * @name ExtensionRegisteriOSUIApplicationDelegate
 * @language C
 * @param delegate [type:void*] An id<UIApplicationDelegate>.
 * @note Register before UIApplicationMain to receive launch callbacks. Registration
 * does not retain the delegate; keep it alive until it is unregistered.
 * This function is only available on iOS. [icon:ios]
 */

/*# Unregister an application delegate
 * Deregister a previously registered iOS application delegate.
 * This function is only available on iOS. [icon:ios]
 * @name ExtensionUnregisteriOSUIApplicationDelegate
 * @language C
 * @param delegate [type:void*] An id<UIApplicationDelegate>.
 */

/*# Register a scene delegate
 * Register an iOS scene observer. Multiple delegates are supported (Max 32).
 * Defold owns the single game window and its scene configuration. Registered
 * observers receive scene connection/disconnection, activation/deactivation,
 * foreground/background, openURLContexts, continueUserActivity,
 * willContinueUserActivityWithType, didFailToContinueUserActivityWithType:error:
 * and didUpdateUserActivity callbacks. Other optional delegate methods are not
 * forwarded. No legacy UIApplicationDelegate callbacks are synthesized.
 *
 * Register before UIApplicationMain (for example in a static constructor) to
 * receive the initial scene connection. Extension app-initialize is too late.
 * Cold-launch URLs, user activities and notification responses are available in
 * UISceneConnectionOptions. Scene connection may precede Lua initialization;
 * extensions must retain any data they need to deliver to Lua later.
 * @name ExtensionRegisteriOSUISceneDelegate
 * @language C
 * @param delegate [type:void*] An id<UISceneDelegate>.
 * @note Register and unregister on the main thread. Registration does not retain
 * the delegate; keep it alive until unregistering. Each event retains a snapshot
 * of its observers, so unregistering during delivery affects subsequent events.
 * Duplicate registrations are ignored. Late registration does not replay events.
 * This function is only available on iOS. [icon:ios]
 * @examples
 * ```objective-c
 * // myextension_ios.mm
 * @interface MySceneDelegate : NSObject <UISceneDelegate>
 * - (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options;
 * - (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts;
 * @end
 *
 * struct MySceneDelegateRegistration
 * {
 *     MySceneDelegate* m_Delegate;
 *     MySceneDelegateRegistration()
 *     {
 *         m_Delegate = [[MySceneDelegate alloc] init];
 *         dmExtension::RegisteriOSUISceneDelegate(m_Delegate);
 *     }
 *     ~MySceneDelegateRegistration()
 *     {
 *         dmExtension::UnregisteriOSUISceneDelegate(m_Delegate);
 *         [m_Delegate release];
 *     }
 * };
 * MySceneDelegateRegistration g_MySceneDelegateRegistration;
 * ```
 */

/*# Unregister a scene delegate
 * Deregister a previously registered iOS scene observer on the main thread.
 * This function is only available on iOS. [icon:ios]
 * @name ExtensionUnregisteriOSUISceneDelegate
 * @language C
 * @param delegate [type:void*] An id<UISceneDelegate>.
 */

/*# declare an extension
 * Declare an extension using its identifier and register/unregister
 * callbacks. The identifier is also its exported registration symbol and name.
 * Register configures lifecycle callbacks on the opaque extension handle, before
 * AppInitialize. Unregister runs after AppFinalize. Both repeat on engine reboot.
 *
 * The legacy eight-argument form remains supported and emits a compiler
 * deprecation warning: DM_DECLARE_EXTENSION(symbol, name, app_init, app_final,
 * init, update, on_event, final). It keeps its callbacks and ABI. Do not add new
 * callbacks to that form; use setters through the three-argument form.
 * @macro
 * @name DM_DECLARE_EXTENSION
 * @language C
 * @examples
 * ```c
 * static ExtensionResult RegisterMyExtension(HExtension extension)
 * {
 *     ExtensionSetUpdateFn(extension, UpdateMyExtension);
 *     return EXTENSION_RESULT_OK;
 * }
 * DM_DECLARE_EXTENSION(MyExtension, RegisterMyExtension, 0);
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
