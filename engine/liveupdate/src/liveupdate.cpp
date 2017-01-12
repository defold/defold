#include "liveupdate.h"
#include "liveupdate_private.h"

#include <string.h>

#include <ddf/ddf.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <resource/resource.h>
#include <resource/resource_archive.h>

namespace dmLiveUpdate
{

    const int MAX_MANIFESTS = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;

    struct LiveUpdate
    {
        LiveUpdate()
        {
            memset(this, 0x0, sizeof(*this));
        }

        dmResource::Manifest* m_Manifest;
        dmResource::Manifest* m_Manifests[MAX_MANIFESTS];
    };

    LiveUpdate g_LiveUpdate;

    /** ***********************************************************************
     ** LiveUpdate utility functions
     ********************************************************************** **/

    void HashToString(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* hash, char* buf, uint32_t buflen)
    {
        const uint32_t hlen = HashLength(algorithm);
        if (buf != NULL && buflen > 0)
        {
            buf[0] = 0x0;
            for (uint32_t i = 0; i < hlen; ++i)
            {
                char current[3];
                DM_SNPRINTF(current, 3, "%02x\0", hash[i]);
                dmStrlCat(buf, current, buflen);
            }
        }
    }

    uint32_t GetMissingResources(const char* path, char*** buffer)
    {
        uint32_t resourceCount = MissingResources(g_LiveUpdate.m_Manifest, path, NULL, 0);
        if (resourceCount > 0)
        {
            uint8_t** resources = (uint8_t**) malloc(resourceCount * sizeof(uint8_t*));
            *buffer = (char**) malloc(resourceCount * sizeof(char**));
            MissingResources(g_LiveUpdate.m_Manifest, path, resources, resourceCount);

            dmLiveUpdateDDF::HashAlgorithm algorithm = g_LiveUpdate.m_Manifest->m_DDF->m_Data.m_Header.m_ResourceHashAlgorithm;
            uint32_t hexDigestLength = HexDigestLength(algorithm) + 1;
            for (uint32_t i = 0; i < resourceCount; ++i)
            {
                (*buffer)[i] = (char*) malloc(hexDigestLength * sizeof(char*));
                HashToString(algorithm, resources[i], (*buffer)[i], hexDigestLength);
            }

            free(resources);
        }

        return resourceCount;
    }

    static int addManifest(dmResource::Manifest* manifest)
    {
        for (int i = 0; i < MAX_MANIFESTS; ++i)
        {
            if (g_LiveUpdate.m_Manifests[i] == 0x0)
            {
                g_LiveUpdate.m_Manifests[i] = manifest;
                return i;
            }
        }

        return -1;
    }

    static bool removeManifest(int manifestIndex)
    {
        if (manifestIndex >= 0 && manifestIndex < MAX_MANIFESTS)
        {
            if (g_LiveUpdate.m_Manifests[manifestIndex] != 0x0)
            {
                delete g_LiveUpdate.m_Manifests[manifestIndex];
                g_LiveUpdate.m_Manifests[manifestIndex] = 0x0;
                return true;
            }
        }

        return false;
    }


    /** ***********************************************************************
     ** Lua API functions
     ********************************************************************** **/

    int LiveUpdate_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CURRENT_MANIFEST);
        return 1;
    }

    int LiveUpdate_CreateManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* manifestData = luaL_checkstring(L, 1);

        dmResource::Manifest* manifest = new dmResource::Manifest();
        // TODO: Create manifest from manifestData
        int manifestIndex = addManifest(manifest);

        if (manifestIndex == -1)
        {
            delete manifest;
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest buffer is full (%d/%d)", MAX_MANIFESTS, MAX_MANIFESTS);
        }

        lua_pushnumber(L, manifestIndex);
        assert(top == (lua_gettop(L) + 1));
        return 1;
    }

    int LiveUpdate_DestroyManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        int manifestIndex = luaL_checkint(L, 1);

        if (!removeManifest(manifestIndex))
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    int LiveUpdate_MissingResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* path = luaL_checkstring(L, 1);

        char** buffer = 0x0;
        uint32_t resourceCount = GetMissingResources(path, &buffer);

        lua_newtable(L);
        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            lua_pushnumber(L, i + 1);
            lua_pushstring(L, buffer[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    int LiveUpdate_VerifyResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_StoreResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }


    /** ***********************************************************************
     ** Engine integration
     ********************************************************************** **/

    static const luaL_reg LiveUpdate_methods[] =
    {
        {"get_current_manifest", LiveUpdate_GetCurrentManifest},
        {"create_manifest", LiveUpdate_CreateManifest},
        {"destroy_manifest", LiveUpdate_DestroyManifest},
        {"missing_resources", LiveUpdate_MissingResources},
        {"verify_resource", LiveUpdate_VerifyResource},
        {"store_resource", LiveUpdate_StoreResource},
        {0, 0}
    };

    void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, LIB_NAME, LiveUpdate_methods);

        lua_pop(L, 1);
    }

    void Initialize(const dmGameSystem::ScriptLibContext& context)
    {
        g_LiveUpdate.m_Manifest = dmResource::GetManifest(context.m_Factory);
        dmLiveUpdate::LuaInit(context.m_LuaState);
    }

    void Finalize(const dmGameSystem::ScriptLibContext& context)
    {
        g_LiveUpdate.m_Manifest = 0x0;
    }

};
