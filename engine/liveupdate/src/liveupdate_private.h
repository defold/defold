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

    uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm);

    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm);

    HResourceEntry FindResourceEntry(const HManifestFile manifest, const char* path);

    uint32_t MissingResources(dmResource::Manifest* manifest, const char* path, uint8_t* entries[], uint32_t entries_size);

    void HashToString(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* hash, char* buf, uint32_t buflen);

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, uint32_t buflen, uint8_t* digest);

};

#endif // H_LIVEUPDATE_PRIVATE