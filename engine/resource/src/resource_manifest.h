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

#ifndef DM_RESOURCE_MANIFEST_H
#define DM_RESOURCE_MANIFEST_H

#include <stdint.h>
#include "resource.h"
#include <dlib/hash.h>

namespace dmURI
{
    struct Parts;
}

// TODO: Hide the implementation
namespace dmLiveUpdateDDF
{
    struct ManifestFile;
    struct ManifestData;
    struct ResourceEntry;
}

namespace dmResource
{
    const static uint32_t MANIFEST_VERSION = 0x05; // also dictates version over live update data (e.g. LiveUpdateResourceHeader)

    // TODO: Hide the implementation
    struct Manifest
    {
        Manifest()
        {
            memset(this, 0, sizeof(Manifest));
        }

        dmResourceArchive::HArchiveIndexContainer   m_ArchiveIndex;
        dmLiveUpdateDDF::ManifestFile*              m_DDF;
        dmLiveUpdateDDF::ManifestData*              m_DDFData;
    };

    // Get the string representation of the Sha1 hash of the "project.title" from game.project
    const char*         GetProjectId(dmResource::Manifest* manifest, char* buffer, uint32_t buffer_size);

    const char*         GetManifestPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size); // .dmanifest

    uint32_t            GetEntryHashLength(dmResource::Manifest* manifest);

    void                DeleteManifest(dmResource::Manifest* manifest);
    dmResource::Result  LoadManifest(const dmURI::Parts* uri, dmResource::Manifest** out);
    dmResource::Result  LoadManifest(const char* path, dmResource::Manifest** out);
    dmResource::Result  LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out);

    dmResource::Result  WriteManifest(const char* path, dmResource::Manifest* manifest);

    // Gets the dependencies of a resource.
    // Note: Only returns the dependency path urls. It doesn't recurse.
    dmResource::Result  GetDependencies(dmResource::Manifest* manifest, const dmhash_t url_hash, dmArray<dmhash_t>& dependencies);
    //dmResource::Result  GetDependencies(dmResource::Manifest* manifest, const dmhash_t url_hash, dmArray<dmLiveUpdateDDF::ResourceEntry*>& dependencies);

    /*#
     * Find resource entry within the manifest
     * @param archive archive index handle
     * @param hash resource hash digest to find
     * @param entry entry data
     * @return entry [type: dmLiveUpdateDDF::ResourceEntry*] the entry if found. 0 otherwise
     */
    dmLiveUpdateDDF::ResourceEntry* FindEntry(dmResource::Manifest* manifest, dmhash_t url_hash);

    // Used when debugging (e.g. unit tests)
    void DebugPrintManifest(dmResource::Manifest* manifest);
}

#endif // DM_RESOURCE_MANIFEST_H
