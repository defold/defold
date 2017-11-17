
// Extension lib defines
#define LIB_NAME "TestExtension"
#define MODULE_NAME "test"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include "frm.h"

typedef uint32_t (*HashFunction)(const char* str, size_t len);
typedef uint32_t (*VersionFunction)(uint32_t* major, uint32_t* middle, uint32_t* minor);

static HashFunction g_HashFunction = 0;
static VersionFunction g_VersionFunction = 0;

static int GetHash(lua_State* L)
{
	DM_LUA_STACK_CHECK(L, 1);

	const char* s = luaL_checkstring(L, 1);

	uint32_t hash = 0;
	if (g_HashFunction) {
		hash = g_HashFunction(s, strlen(s));
	}
	lua_pushinteger(L, hash);
    return 1;
}

static int GetVersion(lua_State* L)
{
	DM_LUA_STACK_CHECK(L, 3);

	uint32_t major = 0;
	uint32_t middle = 0;
	uint32_t minor = 0;

	if (g_VersionFunction) {
		g_VersionFunction(&major, &middle, &minor);
	}
	lua_pushinteger(L, major);
	lua_pushinteger(L, middle);
	lua_pushinteger(L, minor);
    return 3;
}


//#if defined(DM_PLATFORM_IOS)
#if defined (__APPLE__) && (defined(__arm__) || defined(__arm64__))

#include <dlfcn.h>

static void* g_TestLibrary = 0;

static void* LoadLibrary(const char* path)
{
   //char *dylibPath = "/Applications/myapp.app/mydylib2.dylib";

    char buffer[1024];
    GetFrameWorkPath(path, buffer, sizeof(buffer));

    void* library = dlopen(buffer, RTLD_NOW);
    if (library != NULL) {
    	g_HashFunction = (HashFunction)dlsym(library, "GetHash");
    	g_VersionFunction = (VersionFunction)dlsym(library, "GetVersion");

        printf("Functions: GetHash: %p    GetVersion: %p\n", g_HashFunction, g_VersionFunction);
    } else {
        const char* error = dlerror();
        dmLogError("dlopen error: %s", error);
    }

    if (library) {
        printf("Loaded module: %s\n", buffer);
    } else {
        printf("Failed to load module: %s\n", buffer);
    }

    return library;
}

static void UnloadLibrary(void* library)
{
	dlclose(library);
}

dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams* params)
{
	//g_TestLibrary = LoadLibrary("libtestlib.dylib");
    g_TestLibrary = LoadLibrary("libtestlib");

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams* params)
{
	UnloadLibrary(g_TestLibrary);
	g_TestLibrary = 0;
    return dmExtension::RESULT_OK;
}

#else

dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

#endif

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"get_version", GetVersion},
    {"get_hash", GetHash},
    {0, 0}
};


static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}


dmExtension::Result InitializeMyExtension(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}


dmExtension::Result FinalizeMyExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(TestExtension, LIB_NAME, AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, 0, 0, FinalizeMyExtension);
