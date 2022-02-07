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

#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <dlib/hash.h>

namespace dmResource
{
    typedef struct SResourceFactory* HFactory;
    struct Manifest;
}

namespace dmResourceArchive
{
    struct LiveUpdateResource;
}

namespace dmLiveUpdate
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                        =  0,
        RESULT_INVALID_HEADER            = -1,
        RESULT_MEM_ERROR                 = -2,
        RESULT_INVALID_RESOURCE          = -3,
        RESULT_VERSION_MISMATCH          = -4,
        RESULT_ENGINE_VERSION_MISMATCH   = -5,
        RESULT_SIGNATURE_MISMATCH        = -6,
        RESULT_SCHEME_MISMATCH           = -7,
        RESULT_BUNDLED_RESOURCE_MISMATCH = -8,
        RESULT_FORMAT_ERROR              = -9,
        RESULT_IO_ERROR                  = -10,
    };

    const int MAX_MANIFEST_COUNT = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;
    const uint32_t PROJ_ID_LEN = 41; // SHA1 + NULL terminator

    void Initialize(const dmResource::HFactory factory);
    void Finalize();
    void Update();

    void RegisterArchiveLoaders();

    uint32_t GetMissingResources(const dmhash_t urlHash, char*** buffer);

    /*
     * Verifies the manifest cryptographic signature and that the manifest supports the current running dmengine version.
     */
    Result VerifyManifest(const dmResource::Manifest* manifest);

    Result VerifyManifestReferences(const dmResource::Manifest* manifest);

    Result VerifyResource(const dmResource::Manifest* manifest, const char* expected, uint32_t expected_length, const char* data, uint32_t data_length);

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data);

    /*# Registers an archive (.zip) on disc
     */
    Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data);

    Result StoreManifest(dmResource::Manifest* manifest);

    Result ParseManifestBin(uint8_t* manifest_data, uint32_t manifest_len, dmResource::Manifest* manifest);

    dmResource::Manifest* GetCurrentManifest();

    // -1: not using liveupdate
    // 0: single files
    // 1: zip file
    int GetLiveupdateType();

    char* DecryptSignatureHash(const uint8_t* pub_key_buf, uint32_t pub_key_len, uint8_t* signature, uint32_t signature_len, uint32_t* out_digest_len);
};

#endif // DM_LIVEUPDATE_H
