#ifndef DM_EXTENSION
#define DM_EXTENSION

#include <dlib/configfile.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmExtension
{
    enum Result
    {
        RESULT_OK = 0,
    };

    struct Params
    {
        dmConfigFile::HConfig m_ConfigFile;
        lua_State*            m_L;
    };

    // NOTE: Should probably extend Initialize and Finalize
    // with a parameter to a storage location for user-data
    // similar to ComponentCreate in dmGameObject
    struct Desc
    {
        const char* m_Name;
        Result (*Initialize)(Params* params);
        Result (*Finalize)(Params* params);
        const Desc* m_Next;
    };

    const Desc* GetFirstExtension();

    void Register(Desc* desc);

    struct RegisterExtension {
        RegisterExtension(Desc* desc) {
            Register(desc);
        }
    };

#ifdef __GNUC__
    // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported.
    // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
    // The bug only happens when the symbol is on static library though
    #define DM_REGISTER_EXTENSION(name, desc) extern "C" void __attribute__((constructor)) name () { \
        dmExtension::Register(&desc); \
    }
#else
    #define DM_REGISTER_EXTENSION(name, desc) RegisterExtension name(&desc);
#endif

}

#endif // #ifndef DM_EXTENSION


