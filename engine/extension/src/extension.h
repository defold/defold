#ifndef DM_EXTENSION
#define DM_EXTENSION

#include <string.h>
#include <dlib/configfile.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmExtension
{
    /**
     * Results
     */
    enum Result
    {
        RESULT_OK = 0,//!< RESULT_OK
        RESULT_INIT_ERROR = -1,//!< RESULT_INIT_ERROR
    };

    /**
     * Application level initialization parameters
     */
    struct AppParams
    {
        AppParams()
        {
            memset(this, 0, sizeof(*this));
        }
        /// Config file
        dmConfigFile::HConfig m_ConfigFile;
    };

    /**
     * Initialization parameters
     */
    struct Params
    {
        Params()
        {
            memset(this, 0, sizeof(*this));
        }
        /// Config file
        dmConfigFile::HConfig m_ConfigFile;
        /// Lua state
        lua_State*            m_L;
    };

    enum EventID
    {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
    };

    struct Event
    {
        EventID m_Event;
    };

    /**
     * Internal data-structure.
     */
    struct Desc
    {
        const char* m_Name;
        Result (*AppInitialize)(AppParams* params);
        Result (*AppFinalize)(AppParams* params);
        Result (*Initialize)(Params* params);
        Result (*Update)(Params* params);
        void   (*OnEvent)(Params* params, const Event* event);
        Result (*Finalize)(Params* params);
        const Desc* m_Next;
        bool        m_AppInitialized;
    };

    /**
     * Get first extension
     * @return
     */
    const Desc* GetFirstExtension();

    /**
     * Initialize all extends at application level
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result AppInitialize(AppParams* params);

    /**
     * Initialize all extends at application level
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result AppFinalize(AppParams* params);

    /**
     * Dispatches an event to each extension's OnEvent callback
     * @param params parameters
     * @param event the app event
     */
    void DispatchEvent(Params* params, const Event* event);

    /**
     * Register iOS application delegate. Multiple delegates are supported.
     * @note Only available on iOS
     * @param delegate an id<UIApplicationDelegate>
     */
    void RegisterUIApplicationDelegate(void* delegate);

    /**
     * Deregister iOS application delegate
     * @note Only available on iOS
     * @param delegate an id<UIApplicationDelegate>
     */
    void UnregisterUIApplicationDelegate(void* delegate);

    /**
     * Internal function
     * @param desc
     */
    void Register(Desc* desc);

#ifdef __GNUC__
    // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
    // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
    // The bug only happens when the symbol is in a static library though
    #define DM_REGISTER_EXTENSION(name, desc) extern "C" void __attribute__((constructor)) name () { \
        dmExtension::Register(&desc); \
    }
#else
    #define DM_REGISTER_EXTENSION(name, desc) extern "C" void name () { \
        dmExtension::Register(&desc); \
        }\
        int name ## Wrapper(void) { name(); return 0; } \
        __pragma(section(".CRT$XCU",read)) \
        __declspec(allocate(".CRT$XCU")) int (* _Fp ## name)(void) = name ## Wrapper;
#endif

#define DM_EXTENSION_PASTE(x, y) x ## y
#define DM_EXTENSION_PASTE2(x, y) DM_EXTENSION_PASTE(x, y)

/**
 * Declare a new extension
 * @param symbol external symbol extension description
 * @param name extension name. human readble
 * @param appinit app-init function. May be null
 * @param appfinal app-final function. May be null
 * @param init init function. May not be 0
 * @param final final function. May not be 0
 */
#define DM_DECLARE_EXTENSION(symbol, name, appinit, appfinal, init, update, onevent, final) \
        dmExtension::Desc DM_EXTENSION_PASTE2(symbol, __LINE__) = { \
                name, \
                appinit, \
                appfinal, \
                init, \
                update, \
                onevent, \
                final, \
                0, \
                false, \
        };\
        DM_REGISTER_EXTENSION(symbol, DM_EXTENSION_PASTE2(symbol, __LINE__))
}

#endif // #ifndef DM_EXTENSION
