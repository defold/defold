#include "liveupdate.h"
#include "liveupdate_private.h"
#include <string.h>
#include <stdlib.h>

#include <ddf/ddf.h>

#include <dlib/log.h>

namespace dmLiveUpdate
{

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
            char* scratch = (char*) malloc(hexDigestLength * sizeof(char*));
            for (uint32_t i = 0; i < resourceCount; ++i)
            {
                isUnique = true;
                dmResource::HashToString(algorithm, resources[i], scratch, hexDigestLength);
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
            free(scratch);
            free(resources);
        }
        return uniqueCount;
    }

    // Hashes the actual resource data with the same hashing algorithm as spec. by manifest, and compares to the expected resource hash
    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const dmResourceArchive::LiveUpdateResource* resource)
    {
        if (manifest == 0x0 || resource->m_Data == 0x0)
        {
            return false;
        }

        bool result = true;
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) malloc(digestLength * sizeof(uint8_t));
        if (digest == 0x0)
        {
            dmLogError("Failed to allocate memory for hash calculation.");
            return false;
        }

        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) malloc(hexDigestLength * sizeof(char));
        if (hexDigest == 0x0)
        {
            dmLogError("Failed to allocate memory for hash calculation.");
            free(digest);
            return false;
        }

        dmResource::HashToString(algorithm, digest, hexDigest, hexDigestLength);

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

    bool VerifyManifest(dmResource::Manifest* manifest, const uint8_t* manifestData, size_t manifestLen)
    {
        uint8_t* expected_hash_encrypted = manifest->m_DDF->m_Signature.m_Data;
        uint32_t encrypted_len = manifest->m_DDF->m_Signature.m_Count;

         // Don't include manifest file when hashing
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
        dmLogInfo("Signature hash algorithm: %u", algorithm);
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) malloc(digestLength * sizeof(uint8_t));
        if (digest == 0x0)
        {
            dmLogError("Failed to allocate memory for hash calculation.");
            return false;
        }

        dmLogInfo("VerifyManifest, manifest size in bytes; %u", manifest->m_DDF->m_Data.m_Count);
        dmLiveUpdate::CreateManifestHash(algorithm, manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) malloc(hexDigestLength * sizeof(char));
        if (hexDigest == 0x0)
        {
            dmLogError("Failed to allocate memory for hash calculation.");
            free(digest);
            return false;
        }

        dmResource::HashToString(algorithm, digest, hexDigest, hexDigestLength);
        dmLogInfo("Hashed manifest; %s", hexDigest);


        return dmResource::VerifyManifest(manifest) == dmResource::RESULT_OK;
    }

    Result StoreManifest(dmResource::Manifest* manifest)
    {
        if (manifest == 0x0)
        {
            return RESULT_MEM_ERROR;
        }

        dmResource::Result res_result = dmResource::StoreManifest(manifest);
        Result res = (res_result == dmResource::RESULT_OK) ? RESULT_OK : RESULT_INVALID_RESOURCE;
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
        if(!VerifyResource(manifest, (const char*) expected_digest, expected_digest_length, resource))
        {
            dmLogError("Verification failure for Liveupdate archive for resource: %s", expected_digest);
            return RESULT_INVALID_RESOURCE;
        }

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) malloc(digestLength);
        if(digest == 0x0)
        {
            dmLogError("Failed to allocate memory for hash calculation for resource: %s", expected_digest);
            return RESULT_MEM_ERROR;
        }
        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        char proj_id[dmResource::MANIFEST_PROJ_ID_LEN];
        dmResource::HashToString(dmLiveUpdateDDF::HASH_SHA1, manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, proj_id, dmResource::MANIFEST_PROJ_ID_LEN);

        dmResource::Result res = dmResource::NewArchiveIndexWithResource(manifest, digest, digestLength, resource, proj_id, out_new_index);
        free(digest);
        return (res == dmResource::RESULT_OK) ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped)
    {
        dmResourceArchive::SetNewArchiveIndex(archive_container, new_index, mem_mapped);
    }

    int AddManifest(dmResource::Manifest* manifest)
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

    dmResource::Manifest* GetManifest(int manifestIndex)
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

    bool RemoveManifest(int manifestIndex)
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

    void Initialize(const dmResource::HFactory factory)
    {
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
