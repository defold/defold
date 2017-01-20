#include "liveupdate.h"
#include "liveupdate_private.h"

#include <resource/resource.h>
#include <resource/resource_archive.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <axtls/crypto/crypto.h>

namespace dmLiveUpdate
{

    uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
    {
        const uint32_t bitlen[5] = { 0U, 128U, 160U, 256U, 512U };
        return bitlen[(int) algorithm] / 8U;
    }

    uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
    {

        return HashLength(algorithm) * 2U;
    }

    HResourceEntry FindResourceEntry(const HManifestFile manifest, const char* path)
    {
        uint64_t hash = dmHashString64(path);
        HResourceEntry entries = manifest->m_Data.m_Resources.m_Data;

        int first = 0;
        int last = manifest->m_Data.m_Resources.m_Count - 1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            uint64_t currentHash = entries[mid].m_UrlHash;
            if (currentHash == hash)
            {
                return &entries[mid];
            }
            else if (currentHash > hash)
            {
                last = mid - 1;
            }
            else if (currentHash < hash)
            {
                first = mid + 1;
            }
        }

        return NULL;
    }

    uint32_t MissingResources(dmResource::Manifest* manifest, const char* path, uint8_t* entries[], uint32_t entries_size)
    {
        uint32_t resources = 0;
        HResourceEntry entry = FindResourceEntry(manifest->m_DDF, path);
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

    void HashToString(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* hash, char* buf, uint32_t buflen)
    {
        const uint32_t hlen = HashLength(algorithm);
        if (buf != NULL && buflen > 0)
        {
            buf[0] = 0x0;
            for (uint32_t i = 0; i < hlen; ++i)
            {
                char current[3];
                DM_SNPRINTF(current, 3, "%02x\0", hash[i]);
                dmStrlCat(buf, current, buflen);
            }
        }
    }

    void CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const char* buf, uint32_t buflen, uint8_t* digest)
    {
        if (algorithm == dmLiveUpdateDDF::HASH_MD5)
        {
            MD5_CTX context;

            MD5_Init(&context);
            MD5_Update(&context, (const uint8_t*) buf, buflen);
            MD5_Final(digest, &context);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
        {
            SHA1_CTX context;

            SHA1_Init(&context);
            SHA1_Update(&context, (const uint8_t*) buf, buflen);
            SHA1_Final(digest, &context);
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA256)
        {
            dmLogError("The algorithm SHA256 specified for resource hashing is currently not supported");
        }
        else if (algorithm == dmLiveUpdateDDF::HASH_SHA512)
        {
            dmLogError("The algorithm SHA512 specified for resource hashing is currently not supported");
        }
        else
        {
            dmLogError("The algorithm specified for resource hashing is not supported");
        }
    }

};
