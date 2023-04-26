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

#ifndef DM_RESOURCE_PROVIDER_ARCHIVE_PRIVATE_H
#define DM_RESOURCE_PROVIDER_ARCHIVE_PRIVATE_H

#include <dlib/uri.h>

#include "provider.h"
#include "../resource_archive.h" // HArchiveIndexContainer

namespace dmResource
{
    struct Manifest;
}

namespace dmResourceProviderArchivePrivate
{
    const char* GetManifestPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size);       // .dmanifest
    const char* GetArchiveIndexPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size);   // .arci
    const char* GetArchiveDataPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size);   // .arcd

    dmResourceProvider::Result LoadManifest(const dmURI::Parts* uri, dmResource::Manifest** out);
    dmResourceProvider::Result LoadManifest(const char* path, dmResource::Manifest** out);
    dmResourceProvider::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out);

    dmResourceProvider::Result WriteManifest(const char* path, dmResource::Manifest* manifest);

    // For debugging
    void DebugPrintBuffer(const uint8_t* hash, uint32_t len);
    void DebugPrintArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive);
    void DebugPrintManifest(dmResource::Manifest* manifest);
}

#endif // DM_RESOURCE_PROVIDER_ARCHIVE_PRIVATE_H
