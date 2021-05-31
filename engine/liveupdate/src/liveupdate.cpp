// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "liveupdate.h"
#include "liveupdate_private.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <ddf/ddf.h>

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/sys.h>

#include <resource/resource.h>
#include <resource/resource_archive.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{
    const char* LIVEUPDATE_MANIFEST_FILENAME        = "liveupdate.dmanifest";
    const char* LIVEUPDATE_MANIFEST_TMP_FILENAME    = "liveupdate.dmanifest.tmp";
    const char* LIVEUPDATE_INDEX_FILENAME           = "liveupdate.arci";
    const char* LIVEUPDATE_INDEX_TMP_FILENAME       = "liveupdate.arci.tmp";
    const char* LIVEUPDATE_DATA_FILENAME            = "liveupdate.arcd";
    const char* LIVEUPDATE_DATA_TMP_FILENAME        = "liveupdate.arcd.tmp";
    const char* LIVEUPDATE_ARCHIVE_FILENAME         = "liveupdate.ref";
    const char* LIVEUPDATE_ARCHIVE_TMP_FILENAME     = "liveupdate.ref.tmp";
    const char* LIVEUPDATE_BUNDLE_VER_FILENAME      = "bundle.ver";

    static dmResource::Manifest* CreateLUManifest(const dmResource::Manifest* base_manifest);
    static void CreateFilesIfNotExists(dmResourceArchive::HArchiveIndexContainer archive_container, const char* app_support_path, const char* arci, const char* arcd);

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
            case dmResource::RESULT_INVALID_DATA:
                result = RESULT_BUNDLED_RESOURCE_MISMATCH;
                break;
            case dmResource::RESULT_DDF_ERROR:
                result = RESULT_FORMAT_ERROR;
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

        char                        m_AppPath[DMPATH_MAX_PATH];
        dmResource::Manifest*       m_LUManifest;         // the new manifest from StoreManifest
        dmResource::HFactory        m_ResourceFactory;    // Resource system factory
        int                         m_ArchiveType;        // 0: original format, 1: .ref archive, -1: no format used
    };

    LiveUpdate g_LiveUpdate;

    /** ***********************************************************************
     ** LiveUpdate utility functions
     ********************************************************************** **/

    uint32_t GetMissingResources(const dmhash_t urlHash, char*** buffer)
    {
        dmResource::Manifest* manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);

        uint32_t resourceCount = MissingResources(manifest, urlHash, NULL, 0); // First, get the number of dependants
        uint32_t uniqueCount = 0;
        if (resourceCount > 0)
        {
            uint8_t** resources = (uint8_t**) malloc(resourceCount * sizeof(uint8_t*));
            *buffer = (char**) malloc(resourceCount * sizeof(char**));
            resourceCount = MissingResources(manifest, urlHash, resources, resourceCount);

            dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
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

    Result VerifyResource(const dmResource::Manifest* manifest, const char* expected, uint32_t expected_length, const char* data, uint32_t data_length)
    {
        if (manifest == 0x0 || data == 0x0)
        {
            return RESULT_INVALID_RESOURCE;
        }

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength * sizeof(uint8_t));

        CreateResourceHash(algorithm, data, data_length, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) alloca(hexDigestLength * sizeof(char));

        dmResource::BytesToHexString(digest, dmResource::HashLength(algorithm), hexDigest, hexDigestLength);

        bool comp = dmResource::HashCompare((const uint8_t*)hexDigest, hexDigestLength-1, (const uint8_t*)expected, expected_length) == dmResource::RESULT_OK;
        return comp ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    static bool VerifyManifestSupportedEngineVersion(const dmResource::Manifest* manifest)
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

    static Result VerifyManifestSignature(const char* app_path, const dmResource::Manifest* manifest)
    {
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
        uint32_t digest_len = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digest_len * sizeof(uint8_t));

        dmLiveUpdate::CreateManifestHash(algorithm, manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, digest);

        return ResourceResultToLiveupdateResult(dmResource::VerifyManifestHash(app_path, manifest, digest, digest_len));
    }

    static Result VerifyManifestBundledResources(dmResourceArchive::HArchiveIndexContainer archive, const dmResource::Manifest* manifest)
    {
        assert(archive);
        dmResource::Result res;
        if (g_LiveUpdate.m_ResourceFactory) // if the app has already started and is running
        {
            dmMutex::HMutex mutex = dmResource::GetLoadMutex(g_LiveUpdate.m_ResourceFactory);
            while(!dmMutex::TryLock(mutex))
            {
                dmTime::Sleep(100);
            }
            res = dmResource::VerifyResourcesBundled(archive, manifest);
            dmMutex::Unlock(mutex);
        }
        else
        {
            // If we're being called during factory startup,
            // we don't need to protect the archive for new downloaded content at the same time
            res = dmResource::VerifyResourcesBundled(archive, manifest);
        }

        return ResourceResultToLiveupdateResult(res);
    }

    Result VerifyManifest(const dmResource::Manifest* manifest)
    {
        if (!VerifyManifestSupportedEngineVersion(manifest))
            return RESULT_ENGINE_VERSION_MISMATCH;

        return VerifyManifestSignature(g_LiveUpdate.m_AppPath, manifest);
    }

    Result VerifyManifestReferences(const dmResource::Manifest* manifest)
    {
        assert(g_LiveUpdate.m_ResourceFactory != 0);
        const dmResource::Manifest* base_manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);
        // The reason the base manifest is zero here, could be that we're running from the editor.
        // (The editor doesn't bundle the resources required for this step)
        if (!base_manifest)
            return RESULT_INVALID_RESOURCE;
        return VerifyManifestBundledResources(dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory)->m_ArchiveIndex, manifest);
    }

    Result ParseManifestBin(uint8_t* manifest_data, uint32_t manifest_len, dmResource::Manifest* manifest)
    {
        return ResourceResultToLiveupdateResult(dmResource::ManifestLoadMessage(manifest_data, manifest_len, manifest));
    }

    static Result StoreManifestInternal(dmResource::Manifest* manifest)
    {
        char manifest_file_path[DMPATH_MAX_PATH];
        char manifest_tmp_file_path[DMPATH_MAX_PATH];

        char app_support_path[DMPATH_MAX_PATH];
        if (dmResource::RESULT_OK != GetApplicationSupportPath(manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
        {
            return RESULT_IO_ERROR;
        }

        dmPath::Concat(app_support_path, LIVEUPDATE_MANIFEST_FILENAME, manifest_file_path, DMPATH_MAX_PATH);
        dmPath::Concat(app_support_path, LIVEUPDATE_MANIFEST_TMP_FILENAME, manifest_tmp_file_path, DMPATH_MAX_PATH);

        // write to tempfile, if successful move/rename and then delete tmpfile
        dmDDF::Result ddf_result = dmDDF::SaveMessageToFile(manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, manifest_tmp_file_path);
        if (ddf_result != dmDDF::RESULT_OK)
        {
            dmLogError("Failed storing manifest to file '%s', result: %i", manifest_tmp_file_path, ddf_result);
            return RESULT_IO_ERROR;
        }
        dmSys::Result sys_result = dmSys::RenameFile(manifest_file_path, manifest_tmp_file_path);
        if (sys_result != dmSys::RESULT_OK)
        {
            return RESULT_IO_ERROR;
        }
        dmLogInfo("Stored manifest: '%s'", manifest_file_path);
        return RESULT_OK;
    }

    Result StoreManifest(dmResource::Manifest* manifest)
    {
        char app_support_path[DMPATH_MAX_PATH];
        if (dmResource::RESULT_OK != dmResource::GetApplicationSupportPath(manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
        {
            return RESULT_IO_ERROR;
        }

        // Store the manifest file to disc
        return StoreManifestInternal(manifest) == RESULT_OK ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data)
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

    Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data)
    {
        struct stat file_stat;
        bool exists = stat(path, &file_stat) == 0;
        if (!exists) {
            dmLogError("File does not exist: '%s'", path);
            return RESULT_INVALID_RESOURCE;
        }

        AsyncResourceRequest request;
        request.m_CallbackData = callback_data;
        request.m_Callback = callback;
        request.m_Path = path;
        request.m_IsArchive = 1;
        request.m_Manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);
        bool res = AddAsyncResourceRequest(request);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    static void* CopyDDFMessage(void* message, const dmDDF::Descriptor* desc)
    {
        if (!message)
            return 0;

        dmArray<uint8_t> buffer;
        dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(message, desc, buffer);
        if (dmDDF::RESULT_OK != ddf_result)
        {
            return 0;
        }

        void* copy = 0;
        ddf_result = dmDDF::LoadMessage((void*)&buffer[0], buffer.Size(), desc, (void**)&copy);
        if (dmDDF::RESULT_OK != ddf_result)
        {
            return 0;
        }

        return copy;
    }

    static void CreateFilesIfNotExists(dmResourceArchive::HArchiveIndexContainer archive_container, const char* app_support_path, const char* arci, const char* arcd)
    {
        char lu_index_path[DMPATH_MAX_PATH] = {};
        dmPath::Concat(app_support_path, arci, lu_index_path, DMPATH_MAX_PATH);

        struct stat file_stat;
        bool resource_exists = stat(lu_index_path, &file_stat) == 0;

        // If liveupdate.arci does not exists, create it and liveupdate.arcd
        if (!resource_exists)
        {
            FILE* f_lu_index = fopen(lu_index_path, "wb");
            fclose(f_lu_index);
        }

        dmResourceArchive::ArchiveFileIndex* afi = archive_container->m_ArchiveFileIndex;
        if (afi->m_FileResourceData == 0)
        {
            // Data file has same path and filename as index file, but extension .arcd instead of .arci.
            char lu_data_path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, arcd, lu_data_path, DMPATH_MAX_PATH);

            FILE* f_lu_data = fopen(lu_data_path, "ab+");
            if (!f_lu_data)
            {
                dmLogError("Failed to create liveupdate resource file");
            }

            dmResourceArchive::ArchiveFileIndex* afi = archive_container->m_ArchiveFileIndex;

            dmStrlCpy(afi->m_Path, lu_data_path, DMPATH_MAX_PATH);
            dmLogInfo("Live Update archive: %s", afi->m_Path);
            afi->m_FileResourceData = f_lu_data;
            afi->m_ResourceData = 0x0;
            afi->m_ResourceSize = 0;
            afi->m_IsMemMapped = false;
        }
    }

    Result NewArchiveIndexWithResource(const dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index)
    {
        out_new_index = 0x0;
        Result result = VerifyResource(manifest, expected_digest, expected_digest_length, (const char*)resource->m_Data, resource->m_Count);
        if(RESULT_OK != result)
        {
            dmLogError("Verification failure for Liveupdate archive for resource: %s", expected_digest);
            return result;
        }

        char app_support_path[DMPATH_MAX_PATH];
        if (dmResource::RESULT_OK != dmResource::GetApplicationSupportPath(manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
        {
            return RESULT_IO_ERROR;
        }

        // Create empty files if they don't already exist
        // this call might occur before StoreManifest
        CreateFilesIfNotExists(manifest->m_ArchiveIndex, app_support_path, LIVEUPDATE_INDEX_FILENAME, LIVEUPDATE_DATA_FILENAME);

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength);

        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        char index_tmp_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_INDEX_TMP_FILENAME, index_tmp_path, DMPATH_MAX_PATH);

        dmResourceArchive::Result res = dmResourceArchive::NewArchiveIndexWithResource(manifest->m_ArchiveIndex, index_tmp_path, digest, digestLength, resource, app_support_path, out_new_index);

        return (res == dmResourceArchive::RESULT_OK) ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped)
    {
        dmResourceArchive::SetNewArchiveIndex(archive_container, new_index, mem_mapped);
    }

    void SetNewManifest(dmResource::Manifest* manifest)
    {
        dmResource::SetManifest(g_LiveUpdate.m_ResourceFactory, manifest);
    }

    dmResource::Manifest* GetCurrentManifest()
    {
        // A bit lazy, but we don't want to make a copy before we know we'll need it
        if (!g_LiveUpdate.m_LUManifest)
            g_LiveUpdate.m_LUManifest = CreateLUManifest(dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory));

        return g_LiveUpdate.m_LUManifest;
    }

    void Initialize(const dmResource::HFactory factory)
    {
        g_LiveUpdate.m_LUManifest = 0;
        g_LiveUpdate.m_ResourceFactory = factory;
        dmLiveUpdate::AsyncInitialize(factory);
    }

    void Finalize()
    {
        // if this is is not the base manifest
        if (g_LiveUpdate.m_ResourceFactory && dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory) != g_LiveUpdate.m_LUManifest)
            dmResource::DeleteManifest(g_LiveUpdate.m_LUManifest);
        g_LiveUpdate.m_LUManifest = 0;
        g_LiveUpdate.m_ResourceFactory = 0;
        dmLiveUpdate::AsyncFinalize();
    }

    void Update()
    {
        AsyncUpdate();
    }

    // .ref archives take precedence over .arci/.arcd
    // .tmp takes precedence over non .tmp
    //  0: .arci/.arci format
    //  1: .ref format
    // -1: No LU archive file format found
    static int DetermineLUType(const char* app_support_path)
    {
        const char* names[4] = {LIVEUPDATE_ARCHIVE_TMP_FILENAME, LIVEUPDATE_INDEX_TMP_FILENAME, LIVEUPDATE_ARCHIVE_FILENAME, LIVEUPDATE_INDEX_FILENAME};
        int types[4] = {1,0,1,0};
        for (int i = 0; i < 4; ++i)
        {
            char path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, names[i], path, DMPATH_MAX_PATH);
            if (FileExists(path))
            {
                dmLogInfo("Found %s", path);
                return types[i];
            }
        }
        return -1;
    }

    int GetLiveupdateType()
    {
        return g_LiveUpdate.m_ArchiveType;
    }

    static dmResourceArchive::Result LULoadManifest(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* base_manifest, dmResource::Manifest** out)
    {
        dmStrlCpy(g_LiveUpdate.m_AppPath, app_path, DMPATH_MAX_PATH);

        g_LiveUpdate.m_ArchiveType = DetermineLUType(app_support_path);
        if (g_LiveUpdate.m_ArchiveType == -1)
            return dmResourceArchive::RESULT_NOT_FOUND;

        dmResourceArchive::Result result = dmResourceArchive::RESULT_OK;
        if (g_LiveUpdate.m_ArchiveType == 1)
        {
            result = LULoadManifest_Zip(archive_name, app_path, app_support_path, base_manifest, out);
            if (dmResourceArchive::RESULT_OK != result)
            {
                LUCleanup_Zip(archive_name, app_path, app_support_path);
                g_LiveUpdate.m_ArchiveType = 0;
            }
            else
            {
                LUCleanup_Regular(archive_name, app_path, app_support_path);
            }
        }

        if (g_LiveUpdate.m_ArchiveType == 0)
        {
            result = LULoadManifest_Regular(archive_name, app_path, app_support_path, base_manifest, out);
            if (dmResourceArchive::RESULT_OK != result)
            {
                LUCleanup_Regular(archive_name, app_path, app_support_path);
                g_LiveUpdate.m_ArchiveType = -1;
            }
            else
            {
                LUCleanup_Zip(archive_name, app_path, app_support_path);
            }
        }

        if (dmResourceArchive::RESULT_OK == result)
        {
            assert(g_LiveUpdate.m_LUManifest == 0);
            g_LiveUpdate.m_LUManifest = *out;
        }
        return result;
    }

    static dmResourceArchive::Result LUCleanupArchive(const char* archive_name, const char* app_path, const char* app_support_path)
    {
        if (g_LiveUpdate.m_ArchiveType == -1)
            return dmResourceArchive::RESULT_OK;
        else if (g_LiveUpdate.m_ArchiveType == 1)
            return LUCleanup_Zip(archive_name, app_path, app_support_path);
        else
            return LUCleanup_Regular(archive_name, app_path, app_support_path);
    }

    static dmResourceArchive::Result LULoadArchive(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out)
    {
        if (g_LiveUpdate.m_ArchiveType == -1)
            return dmResourceArchive::RESULT_NOT_FOUND;

        dmResourceArchive::Result result;
        if (g_LiveUpdate.m_ArchiveType == 1)
            result = LULoadArchive_Zip(manifest, archive_name, app_path, app_support_path, previous, out);
        else
            result = LULoadArchive_Regular(manifest, archive_name, app_path, app_support_path, previous, out);

        if (dmResourceArchive::RESULT_OK != result)
        {
            LUCleanupArchive(archive_name, app_path, app_support_path);
            g_LiveUpdate.m_ArchiveType = -1;
        }

        return result;
    }

    static dmResourceArchive::Result LUUnloadArchive(dmResourceArchive::HArchiveIndexContainer archive)
    {
        assert(g_LiveUpdate.m_ArchiveType != -1);
        if (g_LiveUpdate.m_ArchiveType == 1)
            return LUUnloadArchive_Zip(archive);
        else
            return LUUnloadArchive_Regular(archive);
    }

    static dmResourceArchive::Result LUFindEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    {
        assert(g_LiveUpdate.m_ArchiveType != -1);
        if (g_LiveUpdate.m_ArchiveType == 1)
            return LUFindEntryInArchive_Zip(archive, hash, hash_len, entry);
        else
            return LUFindEntryInArchive_Regular(archive, hash, hash_len, entry);
    }

    static dmResourceArchive::Result LUReadEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer)
    {
        assert(g_LiveUpdate.m_ArchiveType != -1);
        if (g_LiveUpdate.m_ArchiveType == 1)
            return LUReadEntryFromArchive_Zip(archive, hash, hash_len, entry, buffer);
        else
            return LUReadEntryFromArchive_Regular(archive, hash, hash_len, entry, buffer);
    }

    static dmResource::Manifest* CreateLUManifest(const dmResource::Manifest* base_manifest)
    {
        // Create the actual copy
        dmResource::Manifest* manifest = new dmResource::Manifest;
        manifest->m_DDF = (dmLiveUpdateDDF::ManifestFile*)CopyDDFMessage(base_manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor);
        manifest->m_DDFData = (dmLiveUpdateDDF::ManifestData*)CopyDDFMessage(base_manifest->m_DDFData, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor);

        manifest->m_ArchiveIndex = new dmResourceArchive::ArchiveIndexContainer;
        manifest->m_ArchiveIndex->m_ArchiveIndex = new dmResourceArchive::ArchiveIndex;
        manifest->m_ArchiveIndex->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
        // Even though it isn't technically true, it will allow us to use the correct ArchiveIndex struct
        manifest->m_ArchiveIndex->m_IsMemMapped = 1;

        manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version;
        manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength;
        memcpy(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, sizeof(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5));

        char app_support_path[DMPATH_MAX_PATH];
        if (dmResource::RESULT_OK != dmResource::GetApplicationSupportPath(base_manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
        {
            return 0;
        }

        // Data file has same path and filename as index file, but extension .arcd instead of .arci.
        char lu_data_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_DATA_FILENAME, lu_data_path, DMPATH_MAX_PATH);

        FILE* f_lu_data = fopen(lu_data_path, "wb+");
        if (!f_lu_data)
        {
            dmLogError("Failed to create/load liveupdate resource file");
        }

        dmStrlCpy(manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path, lu_data_path, DMPATH_MAX_PATH);
        dmLogInfo("Live Update archive: %s", manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path);

        manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_FileResourceData = f_lu_data;

        manifest->m_ArchiveIndex->m_Loader.m_Unload = LUUnloadArchive_Regular;
        manifest->m_ArchiveIndex->m_Loader.m_FindEntry = LUFindEntryInArchive_Regular;
        manifest->m_ArchiveIndex->m_Loader.m_Read = LUReadEntryFromArchive_Regular;

        return manifest;
    }

    void RegisterArchiveLoaders()
    {
        dmResourceArchive::ArchiveLoader loader;
        loader.m_LoadManifest = LULoadManifest;
        loader.m_Load = LULoadArchive;
        loader.m_Unload = LUUnloadArchive;
        loader.m_FindEntry = LUFindEntryInArchive;
        loader.m_Read = LUReadEntryInArchive;
        dmResourceArchive::RegisterArchiveLoader(loader);
    }

};
