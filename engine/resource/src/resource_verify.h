// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_RESOURCE_VERIFY_H
#define DM_RESOURCE_VERIFY_H

#include <stdint.h>

#include "resource.h" // Result
#include "resource_archive.h"
#include <resource/liveupdate_ddf.h>

namespace dmResource
{
    typedef struct Manifest* HManifest;

    //Result VerifyManifestReferences(const Manifest* manifest);
    //Result VerifyManifest(const Manifest* manifest);
    Result VerifyManifest(const dmResource::HManifest manifest, const char* public_key_path);

    Result VerifyResource(const dmResource::HManifest manifest, const uint8_t* expected, uint32_t expected_length, const uint8_t* data, uint32_t data_length);

    Result VerifyResourcesBundled(dmResourceArchive::HArchiveIndexContainer base_archive, const Manifest* manifest);

    // Should be private, but is used in unit tests as well
    Result VerifyResourcesBundled(dmLiveUpdateDDF::ResourceEntry* entries, uint32_t num_entries, uint32_t hash_len, dmResourceArchive::HArchiveIndexContainer archive_index);


    // Unit tests ->
    // For testing ascending order
    int VerifyArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive);
}
#endif // DM_RESOURCE_VERIFY_H


