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

namespace dmLiveUpdateDDF
{
    struct ManifestFile;
    struct ManifestData;
    struct ResourceEntry;
}

namespace dmResource
{
    const static uint32_t MANIFEST_VERSION = 0x05; // also dictates version over live update data (e.g. LiveUpdateResourceHeader)

    // For use with GetProjectId()
    const uint32_t MANIFEST_PROJ_ID_LEN = 41; // SHA1 + NULL terminator

    typedef struct Manifest* HManifest;

    // Get the string representation of the Sha1 hash of the "project.title" from game.project
    const char*         GetProjectId(dmResource::Manifest* manifest, char* buffer, uint32_t buffer_size);

    const char*         GetManifestPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size); // .dmanifest

    // Where the liveupdate files are stored "app-support-path/project-id-sha1"
    dmResource::Result  GetApplicationSupportPath(dmResource::HManifest manifest, char* buffer, uint32_t buffer_len);

    uint32_t            GetEntryHashLength(dmResource::HManifest manifest);

    void                DeleteManifest(dmResource::HManifest manifest);
    dmResource::Result  LoadManifest(const dmURI::Parts* uri, dmResource::HManifest* out);
    dmResource::Result  LoadManifest(const char* path, dmResource::HManifest* out);
    dmResource::Result  LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::HManifest* out);

    dmResource::Result  WriteManifest(const char* path, dmResource::HManifest manifest);

    // Gets the dependencies of a resource.
    // Note: Only returns the dependency path urls. It doesn't recurse.
    dmResource::Result  GetDependencies(dmResource::HManifest manifest, const dmhash_t url_hash, dmArray<dmhash_t>& dependencies);

    /*#
     * Get the url hash given a hex digest (the actual filename)
     * @name GetUrlHashFromHexDigest
     * @param manifest [type: dmResource::HManifest] The manifest
     * @param digest_hash [type: dmhash_t] The dmHashBuffer64() of the actual hash digest
     * @param url_path [type: dmhash_t] The url path, or 0 if not exists
     */
    dmhash_t GetUrlHashFromHexDigest(dmResource::HManifest manifest, dmhash_t digest_hash);

    /*#
     * Find resource entry within the manifest
     * @param archive archive index handle
     * @param hash resource hash digest to find
     * @param entry entry data
     * @return entry [type: dmLiveUpdateDDF::ResourceEntry*] the entry if found. 0 otherwise
     */
    dmLiveUpdateDDF::ResourceEntry* FindEntry(dmResource::HManifest manifest, dmhash_t url_hash);

    // Used when debugging (e.g. unit tests)
    void DebugPrintManifest(dmResource::HManifest manifest);
}

#endif // DM_RESOURCE_MANIFEST_H
