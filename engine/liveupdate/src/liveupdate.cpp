#include "liveupdate.h"
#include "liveupdate_private.h"
#include <string.h>

#include <ddf/ddf.h>

#include <resource/resource.h>
#include <resource/resource_archive.h>

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <axtls/crypto/crypto.h>

namespace dmLiveUpdate
{

    const int MAX_MANIFEST_COUNT = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;

    struct LiveUpdate
    {
        LiveUpdate()
        {
            memset(this, 0x0, sizeof(*this));
        }

        dmResource::Manifest* m_Manifest;
        dmResource::Manifest* m_Manifests[MAX_MANIFEST_COUNT];
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

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, uint32_t buflen, uint8_t* digest)
    {
        if (algorithm == dmLiveUpdateDDF::HASH_MD5)
        {
            MD5_CTX context;

            MD5_Init(&context);
            MD5_Update(&context, (const uint8_t*) buf, buflen);
            MD5_Final(digest, &context);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
        {
            SHA1_CTX context;

            SHA1_Init(&context);
            SHA1_Update(&context, (const uint8_t*) buf, buflen);
            SHA1_Final(digest, &context);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA256)
        {
            dmLogError("The algorithm SHA256 specified for resource hashing is currently not supported");
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA512)
        {
            dmLogError("The algorithm SHA512 specified for resource hashing is currently not supported");
        }
        else
        {
            dmLogError("The algorithm specified for resource hashing is not supported");
        }
    }

    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const char* buf, uint32_t buflen)
    {
        bool result = true;
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDF->m_Data.m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = HashLength(algorithm);
        uint8_t* digest = (uint8_t*) malloc(digestLength * sizeof(uint8_t));
        CreateResourceHash(algorithm, buf, buflen, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) malloc(hexDigestLength * sizeof(char));
        HashToString(algorithm, digest, hexDigest, hexDigestLength);

        if (expectedLength == (hexDigestLength - 1))
        {
            for (uint32_t i = 0; i < expectedLength; ++i)
            {
                if (expected[i] != hexDigest[i])
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            result = false;
        }

        free(digest);
        free(hexDigest);
        return result;
    }

    static int AddManifest(dmResource::Manifest* manifest)
    {
        for (int i = 0; i < MAX_MANIFEST_COUNT; ++i)
        {
            if (g_LiveUpdate.m_Manifests[i] == 0x0)
            {
                g_LiveUpdate.m_Manifests[i] = manifest;
                return i;
            }
        }

        return -1;
    }

    static dmResource::Manifest* GetManifest(int manifestIndex)
    {
        if (manifestIndex == CURRENT_MANIFEST)
        {
            return g_LiveUpdate.m_Manifest;
        }

        for (int i = 0; i < MAX_MANIFEST_COUNT; ++i)
        {
            if (i == manifestIndex)
            {
                return g_LiveUpdate.m_Manifests[i];
            }
        }

        return 0x0;
    }

    static bool RemoveManifest(int manifestIndex)
    {
        if (manifestIndex >= 0 && manifestIndex < MAX_MANIFEST_COUNT)
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
        unsigned long manifestLength = 0;
        const char* manifestData = luaL_checklstring(L, 1, &manifestLength);

        dmResource::Manifest* manifest = new dmResource::Manifest();
        dmResource::Result result = dmResource::ParseManifestDDF((uint8_t*) manifestData, manifestLength, manifest->m_DDF);

        if (result != dmResource::RESULT_OK)
        {
            delete manifest;
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest could not be parsed");
        }

        int manifestIndex = AddManifest(manifest);

        if (manifestIndex == -1)
        {
            delete manifest;
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest buffer is full (%d/%d)", MAX_MANIFEST_COUNT, MAX_MANIFEST_COUNT);
        }

        lua_pushnumber(L, manifestIndex);
        assert(lua_gettop(L) == (top + 1));
        return 1;
    }

    int LiveUpdate_DestroyManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        int manifestIndex = luaL_checkint(L, 1);

        if (manifestIndex == CURRENT_MANIFEST)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "Cannot destroy the current manifest");
        }

        if (!RemoveManifest(manifestIndex))
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        assert(lua_gettop(L) == top);
        return 0;
    }

    int LiveUpdate_MissingResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        const char* path = luaL_checkstring(L, 1);

        char** buffer = 0x0;
        uint32_t resourceCount = GetMissingResources(path, &buffer);

        lua_createtable(L, resourceCount, 0);
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
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);

        unsigned long hexDigestLength = 0;
        const char* hexDigest = luaL_checklstring(L, 2, &hexDigestLength);

        unsigned long buflen = 0;
        const char* buf = luaL_checklstring(L, 3, &buflen);

        dmResource::Manifest* manifest = GetManifest(manifestIndex);

        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        bool validResource = VerifyResource(manifest, hexDigest, hexDigestLength, buf, buflen);
        lua_pushboolean(L, (int) validResource);

        assert(lua_gettop(L) == (top + 1));
        return 1;
    }

    int LiveUpdate_StoreResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_VerifyManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    /** ***********************************************************************
     ** Engine integration
     ********************************************************************** **/

    static const luaL_reg Resource_methods[] =
    {
        {"get_current_manifest", LiveUpdate_GetCurrentManifest},
        {"create_manifest", LiveUpdate_CreateManifest},
        {"destroy_manifest", LiveUpdate_DestroyManifest},
        {"verify_resource", LiveUpdate_VerifyResource},
        {"store_resource", LiveUpdate_StoreResource},
        {"verify_manifest", LiveUpdate_VerifyManifest},
        {"store_manifest", LiveUpdate_StoreManifest},
        {0, 0}
    };

    static const luaL_reg CollectionProxy_methods[] =
    {
        {"missing_resources", LiveUpdate_MissingResources},
        {0, 0}
    };

    void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, "resource", Resource_methods);
        lua_pop(L, 1);

        luaL_register(L, "collectionproxy", CollectionProxy_methods);
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
