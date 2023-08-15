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

#ifndef DM_RESOURCE_PROVIDER_ARCHIVE_MUTABLE_H
#define DM_RESOURCE_PROVIDER_ARCHIVE_MUTABLE_H

#include "provider_archive.h"

namespace dmResource
{
    struct Manifest;
}

namespace dmResourceProviderArchiveMutable
{
    // Backwards compatibilty
    dmResourceProvider::Result StoreManifest(dmResource::Manifest* manifest);

    dmResourceProvider::Result StoreResource(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data);

    /*# Registers an archive (.zip) on disc
     */
    Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data, bool verify_archive);

    Result StoreManifest(dmResource::Manifest* manifest);
}

#endif // DM_RESOURCE_PROVIDER_ARCHIVE_MUTABLE_H
