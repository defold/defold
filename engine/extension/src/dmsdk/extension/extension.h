#ifndef DMSDK_EXTENSION
#define DMSDK_EXTENSION

#include <string.h>
#include <dmsdk/dlib/configfile.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}


namespace dmExtension
{
    /**
     * Results
     */
    enum Result
    {
        RESULT_OK = 0,          //!< RESULT_OK
        RESULT_INIT_ERROR = -1, //!< RESULT_INIT_ERROR
    };

    /**
     * Application level initialization parameters
     */
    struct AppParams
    {
        AppParams();
        /// Config file
        dmConfigFile::HConfig m_ConfigFile;
    };

    /**
     * Initialization parameters
     */
    struct Params
    {
        Params();
        /// Config file
        dmConfigFile::HConfig m_ConfigFile;
        /// Lua state
        lua_State*            m_L;
    };

    /**
     * Event id enum
     */
    enum EventID
    {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
    };

    /**
     * Event callback data
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

    /**
     * Declare a new extension
     * @param symbol external symbol extension description
     * @param name extension name. human readble
     * @param appinit app-init function. May be null.
     * @param appfinal app-final function. May be null
     * @param init init function. May not be 0
     * @param update update function. May be null
     * @param on_event event callback function. May be null
     * @param final final function. May not be 0
     *
     * Function signatures
     * Result (*app_init)(AppParams*)
     * Result (*app_finalize)(AppParams*)
     * Result (*initialize)(Params*)
     * Result (*finalize)(Params*)
     * Result (*update)(Params*)
     * void   (*on_event)(Params*, const Event*)
     */
    #define DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final) \
        uint8_t DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__)[dmExtension::m_ExtensionDescBufferSize]; \
        DM_REGISTER_EXTENSION(symbol, DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__), sizeof(DM_EXTENSION_PASTE_SYMREG(symbol, __LINE__)), name, app_init, app_final, init, update, on_event, final);

}

#endif // #ifndef DMSDK_EXTENSION
