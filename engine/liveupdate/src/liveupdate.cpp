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
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/sys.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{

    static const char* LIVEUPDATE_BUNDLE_VER_FILENAME = "bundle.ver";

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

        dmResource::Manifest* m_Manifest;      // the manifest from the resource system
        dmResource::Manifest* m_LUManifest;    // the new manifest from StoreManifest
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

    Result VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expected_length, const char* data, uint32_t data_length)
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

    static bool VerifyManifestSupportedEngineVersion(dmResource::Manifest* manifest)
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

    static Result VerifyManifestSignature(dmResource::Manifest* manifest)
    {
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
        uint32_t digest_len = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digest_len * sizeof(uint8_t));

        dmLiveUpdate::CreateManifestHash(algorithm, manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, digest);

        Result result = ResourceResultToLiveupdateResult(dmResource::VerifyManifestHash(m_ResourceFactory, manifest, digest, digest_len));

        return result;
    }

    static Result VerifyManifestBundledResources(dmResource::Manifest* manifest)
    {
        dmResource::Result res;
        dmMutex::HMutex mutex = dmResource::GetLoadMutex(m_ResourceFactory);
        while(!dmMutex::TryLock(mutex))
        {
            dmTime::Sleep(100);
        }
        res = dmResource::VerifyResourcesBundled(m_ResourceFactory, manifest);
        dmMutex::Unlock(mutex);

        return ResourceResultToLiveupdateResult(res);
    }

    Result VerifyManifest(dmResource::Manifest* manifest)
    {
        if (!VerifyManifestSupportedEngineVersion(manifest))
            return RESULT_ENGINE_VERSION_MISMATCH;

        Result res = VerifyManifestSignature(manifest);

        if (res != RESULT_OK)
            return res;

        return VerifyManifestBundledResources(manifest);
    }

    Result ParseManifestBin(uint8_t* manifest_data, size_t manifest_len, dmResource::Manifest* manifest)
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

        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_MANIFEST_FILENAME, manifest_file_path, DMPATH_MAX_PATH);
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_MANIFEST_TMP_FILENAME, manifest_tmp_file_path, DMPATH_MAX_PATH);

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
        memset(&request, 0, sizeof(request));
        request.m_CallbackData = callback_data;
        request.m_Callback = callback;
        request.m_Path = path;
        request.m_IsArchive = 1;
        request.m_Manifest = dmResource::GetManifest(m_ResourceFactory);
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

    static dmResource::Manifest* CreateLUManifest(dmResource::Manifest* base_manifest)
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
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_DATA_FILENAME, lu_data_path, DMPATH_MAX_PATH);

        FILE* f_lu_data = fopen(lu_data_path, "wb+");
        if (!f_lu_data)
        {
            dmLogError("Failed to create/load liveupdate resource file");
        }

        dmStrlCpy(manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path, lu_data_path, DMPATH_MAX_PATH);
        dmLogInfo("Live Update archive: %s", manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path);

        manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_FileResourceData = f_lu_data;
        return manifest;
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

            // Data file has same path and filename as index file, but extension .arcd instead of .arci.
            char lu_data_path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, arcd, lu_data_path, DMPATH_MAX_PATH);

            FILE* f_lu_data = fopen(lu_data_path, "wb+");
            if (!f_lu_data)
            {
                dmLogError("Failed to create liveupdate resource file");
            }

            if (!archive_container->m_ArchiveFileIndex)
            {
                archive_container->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
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

    Result NewArchiveIndexWithResource(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index)
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
        CreateFilesIfNotExists(manifest->m_ArchiveIndex, app_support_path, dmResource::LIVEUPDATE_INDEX_FILENAME, dmResource::LIVEUPDATE_DATA_FILENAME);

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = dmResource::HashLength(algorithm);
        uint8_t* digest = (uint8_t*) alloca(digestLength);

        CreateResourceHash(algorithm, (const char*)resource->m_Data, resource->m_Count, digest);

        dmResourceArchive::Result res = dmResourceArchive::NewArchiveIndexWithResource(manifest->m_ArchiveIndex, digest, digestLength, resource, app_support_path, out_new_index);

        return (res == dmResourceArchive::RESULT_OK) ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped)
    {
        dmResourceArchive::SetNewArchiveIndex(archive_container, new_index, mem_mapped);
    }

    dmResource::Manifest* GetCurrentManifest()
    {
        // A bit lazy, but we don't want to make a copy before we know we'll need it
        if (!g_LiveUpdate.m_LUManifest)
            g_LiveUpdate.m_LUManifest = CreateLUManifest(g_LiveUpdate.m_Manifest);

        return g_LiveUpdate.m_LUManifest ? g_LiveUpdate.m_LUManifest : g_LiveUpdate.m_Manifest;
    }

    void Initialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        g_LiveUpdate.m_Manifest = dmResource::GetManifest(factory);
        dmLiveUpdate::AsyncInitialize(factory);
    }

    void Finalize()
    {
        // if this is is not the base manifest
        if (g_LiveUpdate.m_Manifest != g_LiveUpdate.m_LUManifest)
            dmResource::DeleteManifest(g_LiveUpdate.m_LUManifest);
        g_LiveUpdate.m_LUManifest = 0;
        g_LiveUpdate.m_Manifest = 0;
        dmLiveUpdate::AsyncFinalize();
    }

    void Update()
    {
        AsyncUpdate();
    }

    static Result BundleVersionValid(const dmResource::Manifest* manifest, const char* bundle_ver_path)
    {
        Result result = RESULT_OK;
        struct stat file_stat;
        bool bundle_ver_exists = stat(bundle_ver_path, &file_stat) == 0;

        uint8_t* signature = manifest->m_DDF->m_Signature.m_Data;
        uint32_t signature_len = manifest->m_DDF->m_Signature.m_Count;
        if (bundle_ver_exists)
        {
            FILE* bundle_ver = fopen(bundle_ver_path, "rb");
            uint8_t* buf = (uint8_t*)alloca(signature_len);
            fread(buf, 1, signature_len, bundle_ver);
            fclose(bundle_ver);
            if (memcmp(buf, signature, signature_len) != 0)
            {
                // Bundle has changed, local liveupdate manifest no longer valid.
                result = RESULT_VERSION_MISMATCH;
            }
        }
        else
        {
            // Take bundled manifest signature and write to 'bundle_ver' file
            FILE* bundle_ver = fopen(bundle_ver_path, "wb");
            size_t bytes_written = fwrite(signature, 1, signature_len, bundle_ver);
            if (bytes_written != signature_len)
            {
                dmLogWarning("Failed to write bundle version to file, wrote %u bytes out of %u bytes.", (uint32_t)bytes_written, signature_len);
            }
            fclose(bundle_ver);
            result = RESULT_OK;
        }

        return result;
    }

    static dmResourceArchive::Result LiveUpdateArchive_LoadManifest(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out)
    {
        char manifest_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_MANIFEST_FILENAME, manifest_path, DMPATH_MAX_PATH);

        struct stat file_stat;
        bool file_exists = stat(manifest_path, &file_stat) == 0;
        if (!file_exists)
            return dmResourceArchive::RESULT_OK;

        // Check if bundle has changed (e.g. app upgraded)
        char bundle_ver_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_BUNDLE_VER_FILENAME, bundle_ver_path, DMPATH_MAX_PATH);
        Result bundle_ver_valid = BundleVersionValid(previous, bundle_ver_path);
        if (bundle_ver_valid != RESULT_OK)
        {
            // Bundle version file exists from previous run, but signature does not match currently loaded bundled manifest.
            // Unlink liveupdate.manifest and bundle_ver_path from filesystem and load bundled manifest instead.
            dmSys::Unlink(bundle_ver_path);
            dmSys::Unlink(manifest_path);

            return dmResourceArchive::RESULT_OK;
        }

        dmResourceArchive::Result result = dmResourceArchive::LoadManifest(manifest_path, out);
        if (dmResourceArchive::RESULT_OK == result)
        {
            assert(g_LiveUpdate.m_LUManifest == 0);
            g_LiveUpdate.m_LUManifest = *out;
        }
        dmLogWarning("Loaded LiveUpdate manifest: %s", manifest_path);
        return result;
    }

    static dmResourceArchive::Result LiveUpdateArchive_Load(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, dmResourceArchive::HArchiveIndexContainer* out)
    {
        char archive_index_tmp_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_INDEX_TMP_FILENAME, archive_index_tmp_path, DMPATH_MAX_PATH);

        char archive_index_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_INDEX_FILENAME, archive_index_path, DMPATH_MAX_PATH);

        struct stat file_stat;
        bool luTempIndexExists = stat(archive_index_tmp_path, &file_stat) == 0;
        if (luTempIndexExists)
        {
            dmSys::Result sys_result = dmSys::RenameFile(archive_index_path, archive_index_tmp_path);
            if (sys_result != dmSys::RESULT_OK)
            {
                // The recently added resources will not be available if we proceed after this point
                dmLogError("Fail to rename '%s' to '%s' (%i).", archive_index_tmp_path, archive_index_path, sys_result);
                return dmResourceArchive::RESULT_IO_ERROR;
            }
            dmSys::Unlink(archive_index_tmp_path);
        }

        bool file_exists = stat(archive_index_path, &file_stat) == 0;
        if (!file_exists)
            return dmResourceArchive::RESULT_OK;

        char archive_data_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, dmResource::LIVEUPDATE_DATA_FILENAME, archive_data_path, DMPATH_MAX_PATH);

        return dmResourceArchive::LoadArchive(archive_index_path, archive_data_path, out);
    }

    static dmResourceArchive::Result LiveUpdateArchive_Unload(dmResourceArchive::HArchiveIndexContainer archive)
    {
        dmResource::UnmountArchiveInternal(archive, archive->m_UserData);
        return dmResourceArchive::RESULT_OK;
    }

    void RegisterArchiveLoaders()
    {
        dmResourceArchive::ArchiveLoader loader;
        loader.m_LoadManifest = LiveUpdateArchive_LoadManifest;
        loader.m_Load = LiveUpdateArchive_Load;
        loader.m_Unload = LiveUpdateArchive_Unload;
        loader.m_FindEntry = dmResourceArchive::FindEntryInArchive;
        loader.m_Read = dmResourceArchive::ReadEntryFromArchive;
        dmResourceArchive::RegisterArchiveLoader(loader);
    }

};
