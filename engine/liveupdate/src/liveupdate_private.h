#ifndef H_LIVEUPDATE_PRIVATE
#define H_LIVEUPDATE_PRIVATE

#define LIB_NAME "liveupdate"

#include <resource/manifest_ddf.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{

    typedef dmLiveUpdateDDF::ManifestFile* HManifestFile;
    typedef dmLiveUpdateDDF::ResourceEntry* HResourceEntry;

    void LuaInit(lua_State* L);

    int LiveUpdate_GetCurrentManifest(lua_State* L);
    int LiveUpdate_CreateManifest(lua_State* L);
    int LiveUpdate_DestroyManifest(lua_State* L);

    int LiveUpdate_MissingResources(lua_State* L);

    int LiveUpdate_VerifyResource(lua_State* L);
    int LiveUpdate_StoreResource(lua_State* L);

    int LiveUpdate_VerifyManifest(lua_State* L);
    int LiveUpdate_StoreManifest(lua_State* L);

    uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm);
    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm);
    HResourceEntry FindResourceEntry(const HManifestFile manifest, const char* path);
    uint32_t MissingResources(dmResource::Manifest* manifest, const char* path, uint8_t* entries[], uint32_t entries_size);

};

#endif // H_LIVEUPDATE_PRIVATE