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

#ifndef H_LIVEUPDATE_PRIVATE
#define H_LIVEUPDATE_PRIVATE

#define LIB_NAME "liveupdate"

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
    struct AsyncResourceRequest
    {
        dmResource::Manifest*   m_Manifest;
        uint32_t                m_ExpectedResourceDigestLength;
        const char*             m_ExpectedResourceDigest;
        dmResourceArchive::LiveUpdateResource m_Resource;
        const char*             m_Path;
        void*                   m_CallbackData;
        uint8_t                 m_IsArchive:1;
        void (*m_Callback)(bool,void*);
    };

    struct ResourceRequestCallbackData
    {
        void* m_CallbackData;
        void (*m_Callback)(bool, void*);
        dmResourceArchive::HArchiveIndexContainer m_ArchiveIndexContainer;
        dmResourceArchive::HArchiveIndex m_NewArchiveIndex;
        bool m_Status;
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

    // Verifies a zip archive
    Result VerifyArchive(const char* path);
    // Stages a zip archive for loading after a reboot
    Result StoreArchive(const char* path);
};

#endif // H_LIVEUPDATE_PRIVATE

