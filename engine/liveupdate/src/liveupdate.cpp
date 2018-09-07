#include "liveupdate.h"
#include "liveupdate_private.h"
#include <string.h>
#include <stdlib.h>

#include <ddf/ddf.h>

#include <dlib/log.h>
#include <dlib/sys.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{
    Result ResourceResultToLiveupdateResult(dmResource::Result r)
    {
        Result result;
        switch (r)
        {
            case dmResource::RESULT_OK:
                result = RESULT_OK;
                break;
            case dmResource::RESULT_IO_ERROR:
                result = RESULT_INVALID_RESOURCE;
                break;
            case dmResource::RESULT_FORMAT_ERROR:
                result = RESULT_INVALID_RESOURCE;
                break;
            case dmResource::RESULT_VERSION_MISMATCH:
                result = RESULT_VERSION_MISMATCH;
                break;
            case dmResource::RESULT_SIGNATURE_MISMATCH:
                result = RESULT_SIGNATURE_MISMATCH;
                break;
            case dmResource::RESULT_NOT_SUPPORTED:
                result = RESULT_SCHEME_MISMATCH;
                break;
            default:
                result = RESULT_INVALID_RESOURCE;
                break;
        }
        return result;
    }

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
    /// Resource system factory
    static dmResource::HFactory m_ResourceFactory = 0x0;

    /** ***********************************************************************
     ** LiveUpdate utility functions
     ********************************************************************** **/

    uint32_t GetMissingResources(const dmhash_t urlHash, char*** buffer)
    {
        uint32_t resourceCount = MissingResources(g_LiveUpdate.m_Manifest, urlHash, NULL, 0);
        uint32_t uniqueCount = 0;
        if (resourceCount > 0)
        {
            uint8_t** resources = (uint8_t**) malloc(resourceCount * sizeof(uint8_t*));
            *buffer = (char**) malloc(resourceCount * sizeof(char**));
            MissingResources(g_LiveUpdate.m_Manifest, urlHash, resources, resourceCount);

            dmLiveUpdateDDF::HashAlgorithm algorithm = g_LiveUpdate.m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
            uint32_t hexDigestLength = HexDigestLength(algorithm) + 1;
            bool isUnique;
            char* scratch = (char*) alloca(hexDigestLength * sizeof(char*));
            for (uint32_t i = 0; i < resourceCount; ++i)
            {
                isUnique = true;
                dmResource::BytesToHexString(resources[i], dmResource::HashLength(algorithm), scratch, hexDigestLength);
                for (uint32_t j = 0; j < uniqueCount; ++j) // only return unique hashes even if there are multiple resource instances in the collectionproxy
                {
                    if (memcmp((*buffer)[j], scratch, hexDigestLength) == 0)
                    {
                        isUnique = false;
                        break;
                    }
                }
                if (isUnique)
                {
                    (*buffer)[uniqueCount] = (char*) malloc(hexDigestLength * sizeof(char*));
                    memcpy((*buffer)[uniqueCount], scratch, hexDigestLength);
                    ++uniqueCount;
                }
            }
            free(resources);
        }
        return uniqueCount;
    }

    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const dmResourceArchive::LiveUpdateResource* resource)
    {
        if (manifest == 0x0 || resource->m_Data == 0x0)
        {
            return false;
        }

        bool result = true;
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength * sizeof(uint8_t));

        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) alloca(hexDigestLength * sizeof(char));

        dmResource::BytesToHexString(digest, dmResource::HashLength(algorithm), hexDigest, hexDigestLength);

        result = dmResource::HashCompare((const uint8_t*)hexDigest, hexDigestLength-1, (const uint8_t*)expected, expectedLength) == dmResource::RESULT_OK;

        return result;
    }

    bool VerifyManifestSupportedEngineVersion(dmResource::Manifest* manifest)
    {
        // Calculate running dmengine version SHA1 hash
        dmSys::EngineInfo engine_info;
        dmSys::GetEngineInfo(&engine_info);
        bool engine_version_supported = false;
        uint32_t engine_digest_len = dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA1);
        uint8_t* engine_digest = (uint8_t*) alloca(engine_digest_len * sizeof(uint8_t));

        CreateResourceHash(dmLiveUpdateDDF::HASH_SHA1, engine_info.m_Version, strlen(engine_info.m_Version), engine_digest);

        // Compare manifest supported versions to running dmengine version
        dmLiveUpdateDDF::HashDigest* versions = manifest->m_DDFData->m_EngineVersions.m_Data;
        for (uint32_t i = 0; i < manifest->m_DDFData->m_EngineVersions.m_Count; ++i)
        {
            if (memcmp(engine_digest, versions[i].m_Data.m_Data, engine_digest_len) == 0)
            {
                engine_version_supported = true;
                break;
            }
        }

        if (!engine_version_supported)
        {
            dmLogError("Loaded manifest does not support current engine version (%s)", engine_info.m_Version);
        }

        return engine_version_supported;
    }

    Result VerifyManifestSignature(dmResource::Manifest* manifest)
    {
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
        uint32_t digest_len = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digest_len * sizeof(uint8_t));

        dmLiveUpdate::CreateManifestHash(algorithm, manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, digest);

        Result result = ResourceResultToLiveupdateResult(dmResource::VerifyManifestHash(m_ResourceFactory, manifest, digest, digest_len));

        return result;
    }

    Result VerifyManifest(dmResource::Manifest* manifest)
    {
        if (!VerifyManifestSupportedEngineVersion(manifest))
            return RESULT_ENGINE_VERSION_MISMATCH;

        return VerifyManifestSignature(manifest);
    }

    Result ParseManifestBin(uint8_t* manifest_data, size_t manifest_len, dmResource::Manifest* manifest)
    {
        return ResourceResultToLiveupdateResult(dmResource::ManifestLoadMessage(manifest_data, manifest_len, manifest));
    }

    Result StoreManifest(dmResource::Manifest* manifest)
    {
        Result res = dmResource::StoreManifest(manifest) == dmResource::RESULT_OK ? RESULT_OK : RESULT_INVALID_RESOURCE;
        return res;
    }

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(StoreResourceCallbackData*), StoreResourceCallbackData& callback_data)
    {
        if (manifest == 0x0 || resource->m_Data == 0x0)
        {
            return RESULT_MEM_ERROR;
        }

        AsyncResourceRequest request;
        request.m_Manifest = manifest;
        request.m_ExpectedResourceDigestLength = expected_digest_length;
        request.m_ExpectedResourceDigest = expected_digest;
        request.m_Resource.Set(*resource);
        request.m_CallbackData = callback_data;
        request.m_Callback = callback;
        bool res = AddAsyncResourceRequest(request);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result NewArchiveIndexWithResource(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index)
    {
        out_new_index = 0x0;
        if(!VerifyResource(manifest, expected_digest, expected_digest_length, resource))
        {
            dmLogError("Verification failure for Liveupdate archive for resource: %s", expected_digest);
            return RESULT_INVALID_RESOURCE;
        }

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength);

        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        char proj_id[dmResource::MANIFEST_PROJ_ID_LEN];
        dmResource::BytesToHexString(manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA1), proj_id, dmResource::MANIFEST_PROJ_ID_LEN);
        dmResource::Result res = dmResource::NewArchiveIndexWithResource(manifest, digest, digestLength, resource, proj_id, out_new_index);

        return (res == dmResource::RESULT_OK) ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped)
    {
        dmResourceArchive::SetNewArchiveIndex(archive_container, new_index, mem_mapped);
    }

    dmResource::Manifest* GetCurrentManifest()
    {
        return g_LiveUpdate.m_Manifest;
    }

    void Initialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        g_LiveUpdate.m_Manifest = dmResource::GetManifest(factory);
        dmLiveUpdate::AsyncInitialize(factory);
    }

    void Finalize()
    {
        g_LiveUpdate.m_Manifest = 0x0;
        dmLiveUpdate::AsyncFinalize();
    }

    void Update()
    {
        AsyncUpdate();
    }

};
