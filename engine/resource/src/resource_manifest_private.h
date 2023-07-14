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

#ifndef DM_RESOURCE_MANIFEST_PRIVATE_H
#define DM_RESOURCE_MANIFEST_PRIVATE_H

#include <dlib/hash.h>
#include <dlib/hashtable.h>

namespace dmLiveUpdateDDF
{
    struct ManifestFile;
    struct ManifestData;
}

namespace dmResourceArchive
{
    typedef struct ArchiveIndexContainer* HArchiveIndexContainer;
}

namespace dmResource
{

struct Manifest
{
    Manifest()
    {
        memset(this, 0, sizeof(Manifest));
    }

    dmResourceArchive::HArchiveIndexContainer   m_ArchiveIndex;
    dmLiveUpdateDDF::ManifestFile*              m_DDF;
    dmLiveUpdateDDF::ManifestData*              m_DDFData;
    // For mutable archives, we fill this just-in-time with the mappings from hex digest to url_path
    dmHashTable64<dmhash_t>                     m_DigestToUrl;
};

}

#endif // DM_RESOURCE_MANIFEST_PRIVATE_H
