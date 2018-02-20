#ifndef H_LIVEUPDATE_PRIVATE
#define H_LIVEUPDATE_PRIVATE

#define LIB_NAME "liveupdate"

#include <resource/manifest_ddf.h>
#include <resource/resource_archive.h>
#include <dlib/hash.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{
    struct AsyncResourceRequest
    {
        dmResource::Manifest* m_Manifest;
        uint32_t m_ExpectedResourceDigestLength;
        const char* m_ExpectedResourceDigest;
        dmResourceArchive::LiveUpdateResource m_Resource;
        StoreResourceCallbackData m_CallbackData;
        void (*m_Callback)(StoreResourceCallbackData*);
    };

    struct ResourceRequestCallbackData
    {
        ResourceRequestCallbackData()
        {
            memset(this, 0x0, sizeof(ResourceRequestCallbackData));
        }
        StoreResourceCallbackData m_CallbackData;
        void (*m_Callback)(StoreResourceCallbackData*);
        dmResourceArchive::HArchiveIndexContainer m_ArchiveIndexContainer;
        dmResourceArchive::HArchiveIndex m_NewArchiveIndex;
    };

    typedef dmLiveUpdateDDF::ManifestFile* HManifestFile;
    typedef dmLiveUpdateDDF::ResourceEntry* HResourceEntry;

    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm);

    HResourceEntry FindResourceEntry(const HManifestFile manifest, const dmhash_t urlHash);

    uint32_t MissingResources(dmResource::Manifest* manifest, const dmhash_t urlHash, uint8_t* entries[], uint32_t entries_size);

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, size_t buflen, uint8_t* digest);
    void CreateManifestHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buf, size_t buflen, uint8_t* digest);

    Result NewArchiveIndexWithResource(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index);
    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped);

    void AsyncInitialize(const dmResource::HFactory factory);
    void AsyncFinalize();
    void AsyncUpdate();

    bool AddAsyncResourceRequest(AsyncResourceRequest& request);
};

#endif // H_LIVEUPDATE_PRIVATE

