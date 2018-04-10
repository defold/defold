#ifndef DMSDK_EXTENSION
#define DMSDK_EXTENSION

#include <string.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/align.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}


namespace dmExtension
{
    /*# SDK Extension API documentation
     * [file:<dmsdk/extension/extension.h>]
     *
     * Functions for creating and controlling engine native extension libraries.
     *
     * @document
     * @name Extension
     * @namespace dmExtension
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
    enum Result
    {
        RESULT_OK = 0,
        RESULT_INIT_ERROR = -1,
    };

    /*# application level callback data
     *
     * Extension application entry callback data.
     * This is the data structure passed as parameter by extension Application entry callbacks (AppInit and AppFinalize) functions
     *
     * @struct
     * @name dmExtension::AppParams
     * @member m_ConfigFile [type:dmConfigFile::HConfig]
     *
     */
    struct AppParams
    {
        AppParams();
        /// Config file
        dmConfigFile::HConfig m_ConfigFile;
    };

    /*# extension level callback data
     *
     * Extension callback data.
     * This is the data structure passed as parameter by extension callbacks (Init, Finalize, Update, OnEvent)
     *
     * @struct
     * @name dmExtension::Params
     * @member m_ConfigFile [type:dmConfigFile::HConfig]
     * @member m_L [type:lua_State*]
     *
     */
    struct Params
    {
        Params();
        dmConfigFile::HConfig m_ConfigFile;
        lua_State*            m_L;
    };

    /*# event id enumeration
     *
     * Event id enumeration.
     *
     * @enum
     * @name dmExtension::EventID
     * @member dmExtension::EVENT_ID_ACTIVATEAPP
     * @member dmExtension::EVENT_ID_DEACTIVATEAPP
     *
     */
    enum EventID
    {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
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

    /**
     * Extension declaration helper. Internal
     */
    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_EXTENSION(symbol, desc, desc_size, name, app_init, app_final, init, update, on_event, final) extern "C" void __attribute__((constructor)) symbol () { \
            dmExtension::Register((struct dmExtension::Desc*) &desc, desc_size, name, app_init, app_final, init, final, update, on_event); \
        }
    #else
        #define DM_REGISTER_EXTENSION(symbol, desc, desc_size, name, app_init, app_final, init, update, on_event, final) extern "C" void symbol () { \
            dmExtension::Register((struct dmExtension::Desc*) &desc, desc_size, name, app_init, app_final, init, final, update, on_event); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    /**
    * Extension desc bytesize declaration. Internal
    */
    const size_t m_ExtensionDescBufferSize = 128;

    /**
    * Extension declaration helper. Internal
    */
    #define DM_EXTENSION_PASTE_SYMREG(x, y) x ## y

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
        uint8_t DM_ALIGNED(16) DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__)[dmExtension::m_ExtensionDescBufferSize]; \
        DM_REGISTER_EXTENSION(symbol, DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__), sizeof(DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__)), name, app_init, app_final, init, update, on_event, final);


    /** Register application delegate
     *
     * Register an iOS application delegate to the engine. Multiple delegates are supported (Max 32)
     * @note Only available on iOS
     * @param delegate an id<UIApplicationDelegate> See https://developer.apple.com/documentation/uikit/uiapplicationdelegate?language=objc
     *
     * @examples
     * ```cpp
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
     * void ExtensionAppInitializeiOS(AppParams* params)
     * {
     *     g_MyApplicationDelegate = [[MyApplicationDelegate alloc] init];
     *     dmExtension::RegisteriOSUIApplicationDelegate(g_MyApplicationDelegate);
     * }
     *
     * ```
     */
    void RegisteriOSUIApplicationDelegate(void* delegate);

    /** Unregister an application delegate
     *
     * Deregister a previously registered iOS application delegate
     * @note Only available on iOS
     * @param delegate an id<UIApplicationDelegate>
     * @examples
     * ```cpp
     * // myextension_ios.mm
     * void ExtensionAppFinalizeiOS(AppParams* params)
     * {
     *     dmExtension::UnregisteriOSUIApplicationDelegate(g_MyApplicationDelegate);
     *     [g_MyApplicationDelegate release];
     *     g_MyApplicationDelegate = 0;
     * }
     *
     */
    void UnregisteriOSUIApplicationDelegate(void* delegate);
}

#endif // #ifndef DMSDK_EXTENSION
