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

#include "liveupdate.h"
#include <resource/liveupdate_ddf.h> // We should eventually move it to the liveupdate library

#include <stdint.h>
#include <dlib/hash.h>


namespace dmLiveUpdate
{

void Initialize(const dmResource::HFactory factory)
{
}

void Finalize()
{
}

void Update()
{
}

void RegisterArchiveLoaders()
{
}

void GetResources(const dmhash_t url_hash, FGetResourceHashHex callback, void* context)
{
}

void GetMissingResources(const dmhash_t url_hash, FGetResourceHashHex callback, void* context)
{
}

Result VerifyManifest(const dmResource::Manifest* manifest)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result VerifyManifestReferences(const dmResource::Manifest* manifest)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result VerifyResource(const dmResource::Manifest* manifest, const char* expected, uint32_t expected_length, const char* data, uint32_t data_length)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data, bool verify_archive)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result StoreManifest(dmResource::Manifest* manifest)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

Result ParseManifestBin(uint8_t* manifest_data, uint32_t manifest_len, dmResource::Manifest* manifest)
{
   return dmLiveUpdate::RESULT_INVALID_RESOURCE;
}

dmResource::Manifest* GetCurrentManifest()
{
   return 0;
}

// -1: not using liveupdate
int GetLiveupdateType()
{
   return -1;
}

// For unit tests

uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
{
    return 40;
}


} // namespace
