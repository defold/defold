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

#include <sys/stat.h>

#include "liveupdate.h"
#include "liveupdate_private.h"

#include <resource/resource.h>
#include <resource/resource_archive.h>
#include <dlib/log.h>
#include <dlib/crypt.h>

namespace dmLiveUpdate
{
    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
    {
        return dmResource::HashLength(algorithm) * 2U;
    }

    HResourceEntry FindResourceEntry(const dmResource::Manifest* manifest, const dmhash_t url_hash)
    {
        HResourceEntry entries = manifest->m_DDFData->m_Resources.m_Data;

        int first = 0;
        int last = manifest->m_DDFData->m_Resources.m_Count - 1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            dmhash_t currentHash = entries[mid].m_UrlHash;
            if (currentHash == url_hash)
            {
                return &entries[mid];
            }
            else if (currentHash > url_hash)
            {
                last = mid - 1;
            }
            else if (currentHash < url_hash)
            {
                first = mid + 1;
            }
        }

        return NULL;
    }

    static HResourceEntry FindEntry(const dmResource::Manifest* manifest, uint32_t index)
    {
        HResourceEntry entries = manifest->m_DDFData->m_Resources.m_Data;
        return &entries[index];
    }

    uint32_t GetNumDependants(dmResource::Manifest* manifest, const dmhash_t url_hash)
    {
        if (manifest == 0x0)
        {
            return 0;
        }

        HResourceEntry entry = FindResourceEntry(manifest, url_hash);
        if (entry == NULL)
        {
            dmLogError("%s: No such resource: %s", __FUNCTION__, dmHashReverseSafe64(url_hash));
            return 0;
        }

        return entry->m_Dependants.m_Count;
    }

    void GetResourceHashes(dmResource::Manifest* manifest, const dmhash_t url_hash, bool only_missing, FGetResourceHash callback, void* context)
    {
        if (manifest == 0x0)
        {
            return;
        }

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t hash_len = dmResource::HashLength(algorithm);

        HResourceEntry entry = FindResourceEntry(manifest, url_hash);
        if (entry == NULL)
        {
            return;
        }

        uint32_t count = entry->m_Dependants.m_Count;

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t index = entry->m_Dependants.m_Data[i];
            HResourceEntry dependant = FindEntry(manifest, index);

            uint8_t* resource_hash = dependant->m_Hash.m_Data.m_Data;
            if (only_missing)
            {
                dmResourceArchive::Result result = dmResourceArchive::FindEntry(manifest->m_ArchiveIndex, resource_hash, hash_len, 0, 0);
                if (result == dmResourceArchive::RESULT_OK)
                    continue;
            }

            callback(context, resource_hash, hash_len);
        }
    }

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, size_t buflen, uint8_t* digest)
    {
        if (algorithm == dmLiveUpdateDDF::HASH_MD5)
        {
            dmCrypt::HashMd5((const uint8_t*)buf, buflen, digest);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
        {
            dmCrypt::HashSha1((const uint8_t*)buf, buflen, digest);
        }
        else
        {
            dmLogError("The algorithm specified for manifest verification hashing is not supported (%i)", algorithm);
        }
    }

    void CreateManifestHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buf, size_t buflen, uint8_t* digest)
    {
        if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
        {
            dmCrypt::HashSha1(buf, buflen, digest);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA256)
        {
            dmCrypt::HashSha256(buf, buflen, digest);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA512)
        {
            dmCrypt::HashSha512(buf, buflen, digest);
        }
        else
        {
            dmLogError("The algorithm specified for manifest verification hashing is not supported (%i)", algorithm);
        }
    }

    bool FileExists(const char* path)
    {
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
    }
};
