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

    HResourceEntry FindResourceEntry(const dmResource::Manifest* manifest, const dmhash_t urlHash)
    {
        HResourceEntry entries = manifest->m_DDFData->m_Resources.m_Data;

        int first = 0;
        int last = manifest->m_DDFData->m_Resources.m_Count - 1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            uint64_t currentHash = entries[mid].m_UrlHash;
            if (currentHash == urlHash)
            {
                return &entries[mid];
            }
            else if (currentHash > urlHash)
            {
                last = mid - 1;
            }
            else if (currentHash < urlHash)
            {
                first = mid + 1;
            }
        }

        return NULL;
    }

    uint32_t MissingResources(dmResource::Manifest* manifest, const dmhash_t urlHash, uint8_t* entries[], uint32_t entries_size)
    {
        uint32_t resources = 0;

        if (manifest == 0x0)
        {
            return 0;
        }

        HResourceEntry entry = FindResourceEntry(manifest, urlHash);
        if (entry != NULL)
        {
            for (uint32_t i = 0; i < entry->m_Dependants.m_Count; ++i)
            {
                uint8_t* resourceHash = entry->m_Dependants.m_Data[i].m_Data.m_Data;
                dmResourceArchive::Result result = dmResourceArchive::FindEntry(manifest->m_ArchiveIndex, resourceHash, NULL);
                if (result != dmResourceArchive::RESULT_OK)
                {
                    if (entries != NULL && resources < entries_size)
                    {
                        entries[resources] = resourceHash;
                    }

                    resources += 1;
                }
            }
        }

        return resources;
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
};
