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

#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/resource/resource.h>

#if defined(__cplusplus)
extern "C" {
#endif
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// C API
typedef enum dmExtensionResult
{
    DM_EXTENSION_RESULT_OK = 0,
    DM_EXTENSION_RESULT_INIT_ERROR = -1,
} dmExtensionResult;

typedef enum dmExtensionEventID
{
    DM_EXTENSION_EVENT_ID_ACTIVATEAPP,
    DM_EXTENSION_EVENT_ID_DEACTIVATEAPP,
    DM_EXTENSION_EVENT_ID_ICONIFYAPP,
    DM_EXTENSION_EVENT_ID_DEICONIFYAPP,
} dmExtensionEventID;

typedef enum dmExtensionCallbackType
{
    DM_EXTENSION_CALLBACK_PRE_RENDER,
    DM_EXTENSION_CALLBACK_POST_RENDER,
} dmExtensionCallbackType;


typedef struct dmExtensionAppParams
{
    dmConfigFileHConfig   m_ConfigFile; // here for backwards compatibility
} dmExtensionAppParams;

void dmExtensionAppParams_Init(dmExtensionAppParams* params);

typedef struct dmExtensionParams
{
    dmConfigFileHConfig     m_ConfigFile;
    dmResourceHFactory      m_ResourceFactory;
    lua_State*              m_L;
} dmExtensionParams;

void dmExtensionParams_Init(struct dmExtensionParams* params);

typedef struct dmExtensionEvent
{
    enum dmExtensionEventID m_Event;
} dmExtensionEvent;

//struct dmExtensionDesc;
typedef dmExtensionResult (*FExtensionAppInit)(dmExtensionAppParams*);
typedef dmExtensionResult (*FExtensionAppFinalize)(dmExtensionAppParams*);
typedef dmExtensionResult (*FExtensionInitialize)(dmExtensionParams*);
typedef dmExtensionResult (*FExtensionFinalize)(dmExtensionParams*);
typedef dmExtensionResult (*FExtensionUpdate)(dmExtensionParams*);
typedef void              (*FExtensionOnEvent)(dmExtensionParams*, const dmExtensionEvent*);

typedef dmExtensionResult (*dmExtensionFCallback)(dmExtensionParams* params);

struct dmExtensionDesc
{
    const struct dmExtensionDesc*   m_Next;
    const char*                     m_Name;
    dmExtensionResult       (*AppInitialize)(dmExtensionAppParams* params);
    dmExtensionResult       (*AppFinalize)(dmExtensionAppParams* params);
    dmExtensionResult       (*Initialize)(dmExtensionParams* params);
    dmExtensionResult       (*Finalize)(dmExtensionParams* params);
    dmExtensionResult       (*Update)(dmExtensionParams* params);
    void                    (*OnEvent)(dmExtensionParams* params, const dmExtensionEvent* event);
    dmExtensionFCallback    PreRender;
    dmExtensionFCallback    PostRender;
    uint8_t                 m_AppInitialized;
};

void dmExtensionRegister(struct dmExtensionDesc* desc,
    uint32_t desc_size,
    const char *name,
    FExtensionAppInit       app_init,
    FExtensionAppFinalize   app_finalize,
    FExtensionInitialize    initialize,
    FExtensionFinalize      finalize,
    FExtensionUpdate        update,
    FExtensionOnEvent       on_event);

int dmExtensionRegisterCallback(dmExtensionCallbackType callback_type, dmExtensionFCallback func);
#if defined(__cplusplus)
} // extern "C"
#endif

#if defined(__cplusplus)
namespace dmExtension
{
    /*# SDK Extension API documentation
     *
     * Functions for creating and controlling engine native extension libraries.
     *
     * @document
     * @name Extension
     * @namespace dmExtension
     * @path engine/dlib/src/dmsdk/extension/extension.h
     */

    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name dmExtension::Result
     * @member dmExtension::RESULT_OK
     * @member dmExtension::RESULT_INIT_ERROR
     *
     */
    //typedef dmExtensionResult Result;
    enum Result
    {
        RESULT_OK           = DM_EXTENSION_RESULT_OK,
        RESULT_INIT_ERROR   = DM_EXTENSION_RESULT_INIT_ERROR,
    };

    /*# application level callback data
     *
     * Extension application entry callback data.
     * This is the data structure passed as parameter by extension Application entry callbacks (AppInit and AppFinalize) functions
     *
     * @note This struct is kept as-is for backwards compatibility. In practice, the struct passed to the callbacks is of the type dmEngine::ExtensionAppParams
     *
     * @struct
     * @name dmExtension::AppParams
     * @member m_ConfigFile [type:dmConfigFile::HConfig]
     *
     */
    typedef dmExtensionAppParams AppParams;
    // struct AppParams
    // {
    //     AppParams();
    //     dmConfigFile::HConfig   m_ConfigFile; // here for backwards compatibility
    // };

    /*# extension level callback data
     *
     * Extension callback data.
     * This is the data structure passed as parameter by extension callbacks (Init, Finalize, Update, OnEvent)
     *
     * @struct
     * @name dmExtension::Params
     * @member m_ConfigFile [type: dmConfigFile::HConfig] the config file
     * @member m_ResourceFactory [type: dmResource::HFactory] the resource factory
     * @member m_L [type: lua_State*] the lua state
     *
     */
    typedef dmExtensionParams Params;


    /*# event id enumeration
     *
     * Event id enumeration.
     *
     * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
     *
     * @enum
     * @name dmExtension::EventID
     * @member dmExtension::EVENT_ID_ACTIVATEAPP
     * @member dmExtension::EVENT_ID_DEACTIVATEAPP
     * @member dmExtension::EVENT_ID_ICONIFYAPP
     * @member dmExtension::EVENT_ID_DEICONIFYAPP
     *
     */
    enum EventID {
        EVENT_ID_ACTIVATEAPP    = DM_EXTENSION_EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP  = DM_EXTENSION_EVENT_ID_DEACTIVATEAPP,
        EVENT_ID_ICONIFYAPP     = DM_EXTENSION_EVENT_ID_ICONIFYAPP,
        EVENT_ID_DEICONIFYAPP   = DM_EXTENSION_EVENT_ID_DEICONIFYAPP
    };

    /*# extra callback enumeration
     *
     * Extra callback enumeration for RegisterCallback function.
     *
     * @enum
     * @name dmExtension::CallbackType
     * @member dmExtension::CALLBACK_PRE_RENDER
     * @member dmExtension::CALLBACK_POST_RENDER
     *
     */
    enum CallbackType
    {
        CALLBACK_PRE_RENDER     = DM_EXTENSION_CALLBACK_PRE_RENDER,
        CALLBACK_POST_RENDER    = DM_EXTENSION_CALLBACK_POST_RENDER,
    };

    /*# event callback data
     *
     * Extension event callback data.
     * This is the data structure passed as parameter by extension event callbacks (OnEvent)
     *
     * @struct
     * @name dmExtension::Event
     * @member m_Event [type:dmExtension::EventID]
     *
     */
    struct Event
    {
        EventID m_Event;
    };

    /**
     * Extension declaration helper. Internal function. Use DM_DECLARE_EXTENSION
     * @param desc
     * @param desc_size bytesize of buffer holding desc
     * @param name extension name. human readble
     * @param app_init app-init function. May be null
     * @param app_finalize app-final function. May be null
     * @param initialize init function. May not be 0
     * @param finalize function. May not be 0
     * @param update update function. May be null
     * @param on_event event callback function. May be null
     */
    void Register(struct Desc* desc,
        uint32_t desc_size,
        const char *name,
        Result (*app_init)(AppParams*),
        Result (*app_finalize)(AppParams*),
        Result (*initialize)(Params*),
        Result (*finalize)(Params*),
        Result (*update)(Params*),
        void   (*on_event)(Params*, const Event*)
    );

    /*# Extra extension callback typedef
     *
     * Callback typedef for functions passed to RegisterCallback().
     *
     * @typedef
     * @name extension_callback_t
     * @param params [type:Params]
     * @return [type:Result]
     */
    typedef Result (*extension_callback_t)(Params* params);

    /*# Register extra extension callbacks.
     *
     * Register extra extension callbacks.
     *
     * @note Can only be called within the AppInit function for an extension.
     * @name RegisterCallback
     * @param callback_type [type:CallbackType] Callback type enum
     * @param func [type:extension_callback_t] Function to register as callback
     * @return [type:bool] Returns true if successfully registered the function, false otherwise.
     */
    bool RegisterCallback(CallbackType callback_type, extension_callback_t func);

    /**
     * Extension declaration helper. Internal
     */
    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_EXTENSION(symbol, desc, desc_size, name, app_init, app_final, init, update, on_event, final) extern "C" void __attribute__((constructor)) symbol () { \
            dmExtensionRegister((dmExtensionDesc*) &desc, desc_size, name, \
                            (FExtensionAppInit)app_init, \
                            (FExtensionAppFinalize)app_final, \
                            (FExtensionInitialize)init, \
                            (FExtensionFinalize)final, \
                            (FExtensionUpdate)update, \
                            (FExtensionOnEvent)on_event); \
        }
    #else
        #define DM_REGISTER_EXTENSION(symbol, desc, desc_size, name, app_init, app_final, init, update, on_event, final) extern "C" void symbol () { \
            dmExtensionRegister((dmExtensionDesc*) &desc, desc_size, name,  \
                            (FExtensionAppInit)app_init, \
                            (FExtensionAppFinalize)app_final, \
                            (FExtensionInitialize)init, \
                            (FExtensionFinalize)final, \
                            (FExtensionUpdate)update, \
                            (FExtensionOnEvent)on_event); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    /**
    * Extension desc bytesize declaration. Internal
    */
    const size_t m_ExtensionDescBufferSize = 128;

    // internal
    #define DM_EXTENSION_PASTE_SYMREG(x, y) x ## y
    // internal
    #define DM_EXTENSION_PASTE_SYMREG2(x, y) DM_EXTENSION_PASTE_SYMREG(x, y)

    /*# declare a new extension
     *
     * Declare and register new extension to the engine.
     * This macro is used to declare the extension callback functions used by the engine to communicate with the extension.
     *
     * @macro
     * @name DM_DECLARE_EXTENSION
     * @param symbol [type:symbol] external extension symbol description (no quotes).
     * @param name [type:const char*] extension name. Human readable.
     * @param appinit [type:function(dmExtension::AppParams* app_params)] app-init function. May be null.
     *
     * `app_params`
     * : [type:dmExtension::AppParams*] Pointer to an `AppParams` structure.
     *
     * @param appfinal [type:function(dmExtension::AppParams* app_params)] app-final function. May be null.
     *
     * `app_params`
     * : [type:dmExtension::AppParams*] Pointer to an `AppParams` structure.
     *
     * @param init [type:function(dmExtension::Params* params)] init function. May not be null.
     *
     * `params`
     * : [type:dmExtension::Params*] Pointer to a `Params` structure
     *
     * @param update [type:function(dmExtension::Params* params)] update function. May be null.
     *
     * `params`
     * : [type:dmExtension::Params*] Pointer to a `Params` structure
     *
     * @param on_event [type:function(dmExtension::Params* params, const dmExtension::Event* event)] event callback function. May be null.
     *
     * `params`
     * : [type:dmExtension::Params*] Pointer to a `Params` structure
     *
     * `event`
     * : [type:dmExtension::Event*] const Pointer to an `Event` structure
     *
     * @param final [type:function(dmExtension::Params* params)] function. May not be null.
     *
     * `params`
     * : [type:dmExtension::Params*] Pointer to an `Params` structure.
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
        uint8_t DM_ALIGNED(16) DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)[dmExtension::m_ExtensionDescBufferSize]; \
        DM_REGISTER_EXTENSION(symbol, DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)), name, app_init, app_final, init, update, on_event, final);


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
     *         dmExtension::RegisteriOSUIApplicationDelegate(m_Delegate);
     *     }
     *     ~MyAppDelegateRegister() {
     *         dmExtension::UnregisteriOSUIApplicationDelegate(m_Delegate);
     *         [m_Delegate release];
     *     }
     * };
     *
     * MyAppDelegateRegister g_FacebookDelegateRegister;
     * ```
     */
    void RegisteriOSUIApplicationDelegate(void* delegate);

    /*# Unregister an application delegate
     *
     * Deregister a previously registered iOS application delegate
     *
     * This function is only available on iOS. [icon:ios]
     *
     * @name UnregisteriOSUIApplicationDelegate
     * @param delegate an id<UIApplicationDelegate>
     */
    void UnregisteriOSUIApplicationDelegate(void* delegate);
}

#endif // __cplusplus




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


#endif // #ifndef DMSDK_EXTENSION_H
