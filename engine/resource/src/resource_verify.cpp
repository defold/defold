#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#else
#include <alloca.h>
#endif

#include "resource_archive.h"
#include "resource_manifest_private.h"
#include "resource_util.h"
#include "resource_verify.h"

#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/sys.h>

namespace dmResource
{
    // Should be private, but is used in unit tests as well
    dmResource::Result VerifyResourcesBundled(dmLiveUpdateDDF::ResourceEntry* entries, uint32_t num_entries, uint32_t hash_len, dmResourceArchive::HArchiveIndexContainer archive)
    {
        for(uint32_t i = 0; i < num_entries; ++i)
        {
            if (entries[i].m_Flags == dmLiveUpdateDDF::BUNDLED)
            {
                uint8_t* hash = entries[i].m_Hash.m_Data.m_Data;
                dmResourceArchive::Result res = dmResourceArchive::FindEntry(archive, hash, hash_len, 0x0);
                if (res == dmResourceArchive::RESULT_NOT_FOUND)
                {
                    char hash_buffer[64*2+1]; // String repr. of project id SHA1 hash
                    dmResource::BytesToHexString(hash, hash_len, hash_buffer, sizeof(hash_buffer));

                    // Manifest expect the resource to be bundled, but it is not in the archive index.
                    dmLogError("Resource '%s' (%s) is expected to be in the bundle was not found.\nResource was modified between publishing the bundle and publishing the manifest?", entries[i].m_Url, hash_buffer);
                    return dmResource::RESULT_INVALID_DATA;
                }
            }
        }

        return dmResource::RESULT_OK;
    }

    dmResource::Result VerifyResourcesBundled(dmResourceArchive::HArchiveIndexContainer archive, const dmResource::HManifest manifest)
    {
        uint32_t entry_count = manifest->m_DDFData->m_Resources.m_Count;
        dmLiveUpdateDDF::ResourceEntry* entries = manifest->m_DDFData->m_Resources.m_Data;

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t hash_len = dmResource::HashLength(algorithm);

        return VerifyResourcesBundled(entries, entry_count, hash_len, archive);
    }

    Result VerifyResource(const dmResource::HManifest manifest, const uint8_t* expected, uint32_t expected_length, const uint8_t* data, uint32_t data_length)
    {
        if (manifest == 0x0 || data == 0x0)
        {
            return RESULT_INVALID_DATA;
        }

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength * sizeof(uint8_t));

        dmResource::CreateResourceHash(algorithm, data, data_length, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        uint8_t* hexDigest = (uint8_t*) alloca(hexDigestLength * sizeof(uint8_t));

        dmResource::BytesToHexString(digest, dmResource::HashLength(algorithm), (char*)hexDigest, hexDigestLength);
        return dmResource::MemCompare(hexDigest, hexDigestLength-1, expected, expected_length);
    }

    static bool VerifyManifestSupportedEngineVersion(const dmResource::HManifest manifest)
    {
        // Calculate running dmengine version SHA1 hash
        dmSys::EngineInfo engine_info;
        dmSys::GetEngineInfo(&engine_info);
        bool engine_version_supported = false;
        uint32_t engine_digest_len = dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA1);
        uint8_t* engine_digest = (uint8_t*) alloca(engine_digest_len * sizeof(uint8_t));

        dmResource::CreateResourceHash(dmLiveUpdateDDF::HASH_SHA1, (const uint8_t*)engine_info.m_Version, strlen(engine_info.m_Version), engine_digest);

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

    static Result VerifyManifestHash(const char* public_key_path, const HManifest manifest, const uint8_t* expected_digest, uint32_t expected_len)
    {
        Result res = RESULT_OK;
        //char public_key_path[DMPATH_MAX_PATH];
        uint32_t pub_key_size = 0, hash_decrypted_len = 0, out_resource_size = 0;
        uint8_t* pub_key_buf = 0x0;
        uint8_t* hash_decrypted = 0x0;

        // Load public key
        //dmPath::Concat(app_path, "game.public.der", public_key_path, DMPATH_MAX_PATH);
        dmSys::Result sys_res = dmSys::ResourceSize(public_key_path, &pub_key_size);
        if (sys_res != dmSys::RESULT_OK)
        {
            dmLogError("Failed to get size of public key for manifest verification (%i) at path: %s", sys_res, public_key_path);
            free(pub_key_buf);
            return RESULT_IO_ERROR;
        }
        pub_key_buf = (uint8_t*)malloc(pub_key_size);
        assert(pub_key_buf);
        sys_res = dmSys::LoadResource(public_key_path, pub_key_buf, pub_key_size, &out_resource_size);

        if (sys_res != dmSys::RESULT_OK)
        {
            dmLogError("Failed to load public key for manifest verification (%i) at path: %s", sys_res, public_key_path);
            free(pub_key_buf);
            return RESULT_IO_ERROR;
        }

        if (out_resource_size != pub_key_size)
        {
            dmLogError("Failed to load public key for manifest verification at path: %s, tried reading %d bytes, got %d bytes", public_key_path, pub_key_size, out_resource_size);
            free(pub_key_buf);
            return RESULT_IO_ERROR;
        }

        res = dmResource::DecryptSignatureHash(manifest, pub_key_buf, pub_key_size, &hash_decrypted, &hash_decrypted_len);
        if (res != RESULT_OK)
        {
            return res;
        }
        res = dmResource::MemCompare(hash_decrypted, hash_decrypted_len, expected_digest, expected_len);

        free(hash_decrypted);
        free(pub_key_buf);
        return res;
    }

    static Result VerifyManifestSignature(const dmResource::HManifest manifest, const char* public_key_path)
    {
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
        uint32_t digest_len = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digest_len * sizeof(uint8_t));

        dmResource::CreateManifestHash(algorithm, manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, digest);

        return dmResource::VerifyManifestHash(public_key_path, manifest, digest, digest_len);
    }

    Result VerifyManifest(const dmResource::HManifest manifest, const char* public_key_path)
    {
        if (!VerifyManifestSupportedEngineVersion(manifest))
            return RESULT_VERSION_MISMATCH;

        return VerifyManifestSignature(manifest, public_key_path);
    }

    // static Result VerifyManifestBundledResources(dmResourceArchive::HArchiveIndexContainer archive, const dmResource::HManifest manifest)
    // {
    //     assert(archive);
    //     dmResource::Result res;
    //     if (g_LiveUpdate.m_ResourceFactory) // if the app has already started and is running
    //     {
    //         dmMutex::HMutex mutex = dmResource::GetLoadMutex(g_LiveUpdate.m_ResourceFactory);
    //         while(!dmMutex::TryLock(mutex))
    //         {
    //             dmTime::Sleep(100);
    //         }
    //         res = dmResource::VerifyResourcesBundled(archive, manifest);
    //         dmMutex::Unlock(mutex);
    //     }
    //     else
    //     {
    //         // If we're being called during factory startup,
    //         // we don't need to protect the archive for new downloaded content at the same time
    //         res = dmResource::VerifyResourcesBundled(archive, manifest);
    //     }

    //     return ResourceResultToLiveupdateResult(res);
    // }

    // Result VerifyManifestReferences(const dmResource::HManifest manifest)
    // {
    //     assert(g_LiveUpdate.m_ResourceFactory != 0);
    //     const dmResource::HManifest base_manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);
    //     // The reason the base manifest is zero here, could be that we're running from the editor.
    //     // (The editor doesn't bundle the resources required for this step)
    //     if (!base_manifest)
    //         return RESULT_INVALID_RESOURCE;
    //     return VerifyManifestBundledResources(dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory)->m_ArchiveIndex, manifest);
    // }

    // For unit test
    int VerifyArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive)
    {
        dmLogInfo("VerifyArchiveIndex: %p\n", archive);

        uint8_t* hashes = 0;
        //EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            //entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            //uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
            uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            //entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        uint32_t hash_len = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashLength);
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);

        const uint8_t* prevh = hashes;

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            const uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);

            int cmp = memcmp(prevh, h, dmResourceArchive::MAX_HASH);
            if (cmp > 0)
            {
                char hash_buffer1[dmResourceArchive::MAX_HASH*2+1];
                dmResource::BytesToHexString(prevh, hash_len, hash_buffer1, sizeof(hash_buffer1));
                char hash_buffer2[dmResourceArchive::MAX_HASH*2+1];
                dmResource::BytesToHexString(h, hash_len, hash_buffer2, sizeof(hash_buffer2));
                dmLogError("Entry %3d: '%s' is not smaller than", i == 0 ? -1 : i-1, hash_buffer1);
                dmLogError("Entry %3d: '%s'", i, hash_buffer2);
                return cmp;
            }

            prevh = h;
        }
        return 0;
    }
}
