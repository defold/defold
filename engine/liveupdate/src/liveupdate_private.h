// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef H_LIVEUPDATE_PRIVATE
#define H_LIVEUPDATE_PRIVATE

#include "liveupdate.h"
#include <ddf/ddf.h>
#include <resource/liveupdate_ddf.h>
#include <resource/resource_archive.h>
#include <dlib/hash.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{
    extern const char* LIVEUPDATE_MANIFEST_FILENAME;
    extern const char* LIVEUPDATE_MANIFEST_TMP_FILENAME;
    extern const char* LIVEUPDATE_INDEX_FILENAME;
    extern const char* LIVEUPDATE_INDEX_TMP_FILENAME;
    extern const char* LIVEUPDATE_DATA_FILENAME;
    extern const char* LIVEUPDATE_DATA_TMP_FILENAME;
    extern const char* LIVEUPDATE_ARCHIVE_FILENAME;
    extern const char* LIVEUPDATE_ARCHIVE_TMP_FILENAME;
    extern const char* LIVEUPDATE_BUNDLE_VER_FILENAME;

    struct AsyncResourceRequest
    {
        AsyncResourceRequest() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceArchive::LiveUpdateResource m_Resource;
        dmResource::Manifest*       m_Manifest;
        uint32_t                    m_ExpectedResourceDigestLength;
        const char*                 m_ExpectedResourceDigest;
        const char*                 m_Path;
        void*                       m_CallbackData;
        uint8_t                     m_IsArchive:1;
        uint8_t                     m_VerifyArchive:1;
        void (*m_Callback)(bool,void*);
    };

    struct ResourceRequestCallbackData
    {
        void* m_CallbackData;
        void (*m_Callback)(bool, void*);
        dmResourceArchive::HArchiveIndexContainer m_ArchiveIndexContainer;
        dmResourceArchive::HArchiveIndex          m_NewArchiveIndex;
        dmResource::Manifest*                     m_Manifest;
        bool m_Status;
    };

    typedef dmLiveUpdateDDF::ManifestFile* HManifestFile;
    typedef dmLiveUpdateDDF::ResourceEntry* HResourceEntry;

    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm);

    HResourceEntry FindResourceEntry(const HManifestFile manifest, const dmhash_t urlHash);

    uint32_t MissingResources(dmResource::Manifest* manifest, const dmhash_t urlHash, uint8_t* entries[], uint32_t entries_size);

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, size_t buflen, uint8_t* digest);
    void CreateManifestHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buf, size_t buflen, uint8_t* digest);

    Result NewArchiveIndexWithResource(const dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index);
    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped);
    void SetNewManifest(dmResource::Manifest* manifest);

    void AsyncInitialize(const dmResource::HFactory factory);
    void AsyncFinalize();
    void AsyncUpdate();

    bool AddAsyncResourceRequest(AsyncResourceRequest& request);

    bool FileExists(const char* path);

    // regular implementation
    Result BundleVersionValid(const dmResource::Manifest* manifest, const char* bundle_ver_path);
    dmResourceArchive::Result LULoadManifest_Regular(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out);
    dmResourceArchive::Result LULoadArchive_Regular(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out);
    dmResourceArchive::Result LUUnloadArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive);
    dmResourceArchive::Result LUFindEntryInArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry);
    dmResourceArchive::Result LUReadEntryFromArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer);
    dmResourceArchive::Result LUCleanup_Regular(const char* archive_name, const char* app_path, const char* app_support_path);


    // zip archive implementation
    // Verifies and stores a zip archive
    Result StoreZipArchive(const char* path, const bool validate_archive);
    dmResourceArchive::Result LULoadManifest_Zip(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out);
    dmResourceArchive::Result LULoadArchive_Zip(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out);
    dmResourceArchive::Result LUUnloadArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive);
    dmResourceArchive::Result LUFindEntryInArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry);
    dmResourceArchive::Result LUReadEntryFromArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer);
    dmResourceArchive::Result LUCleanup_Zip(const char* archive_name, const char* app_path, const char* app_support_path);

};

#endif // H_LIVEUPDATE_PRIVATE

