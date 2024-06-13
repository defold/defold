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

#ifndef DMSDK_EXTENSION_H
#define DMSDK_EXTENSION_H

#include <stdbool.h>
#include <dmsdk/resource/resource.h>

#include <dmsdk/dlib/align.h> // DM_ALIGNED
#include <dmsdk/dlib/configfile.h>

#if defined(__cplusplus)
extern "C" {
#endif

// TODO: Try to forward declare
// TODO: Try to design away the contents of the context
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>

/*# SDK Extension API documentation
 *
 * Functions for creating and controlling engine native extension libraries.
 *
 * @document
 * @name Extension
 * @path engine/dlib/src/dmsdk/extension/extension.h
 */

/*# result enumeration
 *
 * Result enumeration.
 *
 * @enum
 * @name ExtensionResult
 * @member EXTENSION_RESULT_OK
 * @member EXTENSION_RESULT_INIT_ERROR
 */
typedef enum ExtensionResult
{
    EXTENSION_RESULT_OK = 0,
    EXTENSION_RESULT_INIT_ERROR = -1,
} ExtensionResult;


/*# event id enumeration
 *
 * Event id enumeration.
 *
 * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
 *
 * @enum
 * @name ExtensionEventID
 * @member EXTENSION_EVENT_ID_ACTIVATEAPP
 * @member EXTENSION_EVENT_ID_DEACTIVATEAPP
 * @member EXTENSION_EVENT_ID_ICONIFYAPP
 * @member EXTENSION_EVENT_ID_DEICONIFYAPP
 * @member EXTENSION_EVENT_ID_ENGINE_INITIALIZED
 * @member EXTENSION_EVENT_ID_ENGINE_DELETE
 *
 */
typedef enum ExtensionEventID
{
    EXTENSION_EVENT_ID_ACTIVATEAPP,
    EXTENSION_EVENT_ID_DEACTIVATEAPP,
    EXTENSION_EVENT_ID_ICONIFYAPP,
    EXTENSION_EVENT_ID_DEICONIFYAPP,
    EXTENSION_EVENT_ID_ENGINE_INITIALIZED,
    EXTENSION_EVENT_ID_ENGINE_DELETE,
} ExtensionEventID;

/*# extra callback type
 *
 * Extra callback type for RegisterCallback function.
 *
 * @enum
 * @name ExtensionCallbackType
 * @member EXTENSION_CALLBACK_PRE_RENDER
 * @member EXTENSION_CALLBACK_POST_RENDER
 *
 */
typedef enum ExtensionCallbackType
{
    EXTENSION_CALLBACK_PRE_RENDER,
    EXTENSION_CALLBACK_POST_RENDER,
} ExtensionCallbackType;


typedef struct ExtensionAppParams
{
    HConfigFile m_ConfigFile; // here for backwards compatibility
} ExtensionAppParams;

typedef struct ExtensionParams
{
    HConfigFile         m_ConfigFile;
    HResourceFactory    m_ResourceFactory;
    lua_State*          m_L;
} ExtensionParams;

typedef struct ExtensionEvent
{
    enum ExtensionEventID m_Event;
} ExtensionEvent;


/*#
 * Callback when the app is being initialized. Called before [ref:FExtensionInitialize]
 * @note There is no guarantuee of initialization order. If an extension requires another extension to be initialized,
 *       that should be handled in [ref:FExtensionInitialize].
 * @typedef
 * @name FExtensionAppInitialize
 * @param params [type:ExtensionAppParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */
typedef ExtensionResult (*FExtensionAppInitialize)(ExtensionAppParams*);

/*#
 * Callback when the app is being finalized
 * @typedef
 * @name FExtensionInitialize
 * @param params [type:ExtensionAppParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */
typedef ExtensionResult (*FExtensionAppFinalize)(ExtensionAppParams*);

/*#
 * Callback after all extensions have been called with [ref:FExtensionAppInitialize]
 * @typedef
 * @name FExtensionInitialize
 * @param params [type:ExtensionParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */
typedef ExtensionResult (*FExtensionInitialize)(ExtensionParams*);

/*#
 * Calls for the finalization of an extension
 * @note All extensions will be called with `FExtensionFinalize` before moving on to the next step, the [ref:FExtensionAppFinalize]
 * @typedef
 * @name FExtensionFinalize
 * @param params [type:ExtensionParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */
typedef ExtensionResult (*FExtensionFinalize)(ExtensionParams*);

/*#
 * Updates an extension. Called for each game frame.
 * @typedef
 * @name FExtensionUpdate
 * @param params [type:ExtensionParams]
 * @return result [type:ExtensionResult] EXTENSION_RESULT_OK if all went ok
 */
typedef ExtensionResult (*FExtensionUpdate)(ExtensionParams*);

/*#
 * Receives an event from the engine
 * @typedef
 * @name FExtensionOnEvent
 * @param params [type:ExtensionParams]
 * @param event [type:ExtensionEvent] The current event
 */
typedef void (*FExtensionOnEvent)(ExtensionParams*, const ExtensionEvent*);

/*# Extra extension callback typedef
 *
 * Callback typedef for functions passed to RegisterCallback().
 *
 * @typedef
 * @name FExtensionCallback
 * @param params [type:ExtensionParams]
 * @return [type:ExtensionResult]
 */
typedef ExtensionResult (*FExtensionCallback)(ExtensionParams* params);

/*# Used when registering new extensions
 * @constant
 * @name ExtensionDescBufferSize
 */
const size_t ExtensionDescBufferSize = 128;

/*#
 * Extension declaration helper. Internal function. Use DM_DECLARE_EXTENSION
 * @name ExtensionRegister
 * @param desc
 * @param desc_size [type:const char*] size of buffer holding desc. in bytes
 * @param name [type:const char*] extension name. human readble
 * @param app_initialize [type:FExtensionAppInitialize] app-init function. May be null
 * @param app_finalize [type:FExtensionAppFinalize] app-final function. May be null
 * @param initialize [type:FExtensionInitialize] init function. May not be 0
 * @param finalize [type:FExtensionFinalize] finalize function. May not be 0
 * @param update [type:FExtensionUpdate] update function. May be null
 * @param on_event [type:FExtensionOnEvent] event callback function. May be null
 */
void ExtensionRegister(void* desc,
    uint32_t desc_size,
    const char* name,
    FExtensionAppInitialize app_initialize,
    FExtensionAppFinalize   app_finalize,
    FExtensionInitialize    initialize,
    FExtensionFinalize      finalize,
    FExtensionUpdate        update,
    FExtensionOnEvent       on_event);

/** currently internal
 * Used for registing a pre or post render callback
 */
bool ExtensionRegisterCallback(ExtensionCallbackType callback_type, FExtensionCallback func);

/*# Register application delegate
 *
 * Register an iOS application delegate to the engine. Multiple delegates are supported (Max 32)
 *
 * @note Note that the delegate needs to be registered before the UIApplicationMain in order to
 * handle any earlier callbacks.
 *
 * This function is only available on iOS. [icon:ios]
 *
 * @name RegisteriOSUIApplicationDelegate
 * @param delegate [type:id<UIApplicationDelegate>] An UIApplicationDelegate, see: https://developer.apple.com/documentation/uikit/uiapplicationdelegate?language=objc
 *
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
void ExtensionRegisteriOSUIApplicationDelegate(void* delegate);

/*# Unregister an application delegate
 *
 * Deregister a previously registered iOS application delegate
 *
 * This function is only available on iOS. [icon:ios]
 *
 * @name UnregisteriOSUIApplicationDelegate
 * @param delegate an id<UIApplicationDelegate>
 */
void ExtensionUnregisteriOSUIApplicationDelegate(void* delegate);

// internal
#define DM_EXTENSION_PASTE_SYMREG(x, y) x ## y
// internal
#define DM_EXTENSION_PASTE_SYMREG2(x, y) DM_EXTENSION_PASTE_SYMREG(x, y)

// interal
#define DM_REGISTER_EXTENSION(symbol, desc, desc_size, name, app_init, app_final, init, update, on_event, final) extern "C" void symbol () { \
        ExtensionRegister((void*) &desc, desc_size, name, \
                    (FExtensionAppInitialize)app_init, \
                    (FExtensionAppFinalize)app_final, \
                    (FExtensionInitialize)init, \
                    (FExtensionFinalize)final, \
                    (FExtensionUpdate)update, \
                    (FExtensionOnEvent)on_event); \
    }

/*# declare a new extension
 *
 * Declare and register new extension to the engine.
 * This macro is used to declare the extension callback functions used by the engine to communicate with the extension.
 *
 * @macro
 * @name DM_DECLARE_EXTENSION
 * @param symbol [type:symbol] external extension symbol description (no quotes).
 * @param name [type:const char*] extension name. Human readable.
 * @param app_init [type:function(ExtensionAppParams* app_params)] app-init function. May be null.
 *
 * `app_params`
 * : [type:ExtensionAppParams*] Pointer to an `AppParams` structure.
 *
 * @param app_final [type:function(ExtensionAppParams* app_params)] app-final function. May be null.
 *
 * `app_params`
 * : [type:ExtensionAppParams*] Pointer to an `AppParams` structure.
 *
 * @param init [type:function(ExtensionParams* params)] init function. May not be null.
 *
 * `params`
 * : [type:ExtensionParams*] Pointer to a `Params` structure
 *
 * @param update [type:function(ExtensionParams* params)] update function. May be null.
 *
 * `params`
 * : [type:ExtensionParams*] Pointer to a `Params` structure
 *
 * @param on_event [type:function(ExtensionParams* params, const ExtensionEvent* event)] event callback function. May be null.
 *
 * `params`
 * : [type:ExtensionParams*] Pointer to a `Params` structure
 *
 * `event`
 * : [type:ExtensionEvent*] const Pointer to an `Event` structure
 *
 * @param final [type:function(ExtensionParams* params)] function. May not be null.
 *
 * `params`
 * : [type:ExtensionParams*] Pointer to an `Params` structure.
 *
 * @examples
 *
 * Register a new extension:
 *
 * ```cpp
 * DM_DECLARE_EXTENSION(MyExt, "MyExt", AppInitializeMyExt, AppFinalizeMyExt, InitializeMyExt, UpdateMyExt, OnEventMyExt, FinalizeMyExt);
 * ```
 */
#define DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final) \
    uint8_t DM_ALIGNED(16) DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)[ExtensionDescBufferSize]; \
    DM_REGISTER_EXTENSION(symbol, DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)), name, app_init, app_final, init, update, on_event, final);


/*# Set if the platform is iPhoneOS [icon:ios]
 *
 * Set if the platform is iPhoneOS [icon:ios]
 *
 * @macro
 * @name DM_PLATFORM_IOS
 *
 */

/*# Set if the platform is Android [icon:android]
 *
 * Set if the platform is Android [icon:android]
 *
 * @macro
 * @name DM_PLATFORM_ANDROID
 *
 */

/*# Set if the platform is Html5 [icon:html5]
 *
 * Set if the platform is Html5 [icon:html5]
 *
 * @macro
 * @name DM_PLATFORM_HTML5
 *
 */

/*# Set if the platform is OSX [icon:macos]
 *
 * Set if the platform is OSX [icon:macos]
 *
 * @macro
 * @name DM_PLATFORM_OSX
 *
 */

/*# Set if the platform is Linux [icon:linux]
 *
 * Set if the platform is Linux [icon:linux]
 *
 * @macro
 * @name DM_PLATFORM_LINUX
 *
 */

/*# Set if the platform is Windows [icon:windows] (on both x86 and x86_64)
 *
 * Set if the platform is Windows [icon:windows] (on both x86 and x86_64)
 *
 * @macro
 * @name DM_PLATFORM_WINDOWS
 *
 */

#if defined(__cplusplus)
} // extern "C"

#include "extension.hpp"

#endif
#endif // #ifndef DMSDK_EXTENSION_H
