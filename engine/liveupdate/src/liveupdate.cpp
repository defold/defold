#include "liveupdate.h"
#include "liveupdate_private.h"
#include <string.h>

#include <ddf/ddf.h>

#include <dlib/log.h>

namespace dmLiveUpdate
{

    struct LiveUpdate
    {
        LiveUpdate()
        {
            memset(this, 0x0, sizeof(*this));
        }

        dmResource::Manifest* m_Manifest;
        dmResource::Manifest* m_Manifests[MAX_MANIFEST_COUNT];
    };

    LiveUpdate g_LiveUpdate;

    /** ***********************************************************************
     ** LiveUpdate utility functions
     ********************************************************************** **/

    uint32_t GetMissingResources(const char* path, char*** buffer)
    {
        uint32_t resourceCount = MissingResources(g_LiveUpdate.m_Manifest, path, NULL, 0);
        if (resourceCount > 0)
        {
            uint8_t** resources = (uint8_t**) malloc(resourceCount * sizeof(uint8_t*));
            *buffer = (char**) malloc(resourceCount * sizeof(char**));
            MissingResources(g_LiveUpdate.m_Manifest, path, resources, resourceCount);

            dmLiveUpdateDDF::HashAlgorithm algorithm = g_LiveUpdate.m_Manifest->m_DDF->m_Data.m_Header.m_ResourceHashAlgorithm;
            uint32_t hexDigestLength = HexDigestLength(algorithm) + 1;
            for (uint32_t i = 0; i < resourceCount; ++i)
            {
                (*buffer)[i] = (char*) malloc(hexDigestLength * sizeof(char*));
                HashToString(algorithm, resources[i], (*buffer)[i], hexDigestLength);
            }

            free(resources);
        }

        return resourceCount;
    }

    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const char* buf, uint32_t buflen)
    {
        bool result = true;
        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDF->m_Data.m_Header.m_ResourceHashAlgorithm;
        uint32_t digestLength = HashLength(algorithm);
        uint8_t* digest = (uint8_t*) malloc(digestLength * sizeof(uint8_t));
        CreateResourceHash(algorithm, buf, buflen, digest);

        uint32_t hexDigestLength = digestLength * 2 + 1;
        char* hexDigest = (char*) malloc(hexDigestLength * sizeof(char));
        HashToString(algorithm, digest, hexDigest, hexDigestLength);

        if (expectedLength == (hexDigestLength - 1))
        {
            for (uint32_t i = 0; i < expectedLength; ++i)
            {
                if (expected[i] != hexDigest[i])
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            result = false;
        }

        free(digest);
        free(hexDigest);
        return result;
    }

    int AddManifest(dmResource::Manifest* manifest)
    {
        for (int i = 0; i < MAX_MANIFEST_COUNT; ++i)
        {
            if (g_LiveUpdate.m_Manifests[i] == 0x0)
            {
                g_LiveUpdate.m_Manifests[i] = manifest;
                return i;
            }
        }

        return -1;
    }

    dmResource::Manifest* GetManifest(int manifestIndex)
    {
        if (manifestIndex == CURRENT_MANIFEST)
        {
            return g_LiveUpdate.m_Manifest;
        }

        for (int i = 0; i < MAX_MANIFEST_COUNT; ++i)
        {
            if (i == manifestIndex)
            {
                return g_LiveUpdate.m_Manifests[i];
            }
        }

        return 0x0;
    }

    bool RemoveManifest(int manifestIndex)
    {
        if (manifestIndex >= 0 && manifestIndex < MAX_MANIFEST_COUNT)
        {
            if (g_LiveUpdate.m_Manifests[manifestIndex] != 0x0)
            {
                delete g_LiveUpdate.m_Manifests[manifestIndex];
                g_LiveUpdate.m_Manifests[manifestIndex] = 0x0;
                return true;
            }
        }

        return false;
    }

    void Initialize(const dmResource::HFactory factory)
    {
        g_LiveUpdate.m_Manifest = dmResource::GetManifest(factory);
    }

    void Finalize()
    {
        g_LiveUpdate.m_Manifest = 0x0;
    }

};
