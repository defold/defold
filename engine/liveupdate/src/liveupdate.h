#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <resource/resource.h>
#include <resource/manifest_ddf.h>

namespace dmLiveUpdate
{

    const int MAX_MANIFEST_COUNT = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;

    void Initialize(const dmResource::HFactory factory);
    void Finalize();

    uint32_t GetMissingResources(const char* path, char*** buffer);

    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const char* buf, uint32_t buflen);

    bool StoreResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const char* buf, uint32_t buflen);

    int AddManifest(dmResource::Manifest* manifest);

    dmResource::Manifest* GetManifest(int manifestIndex);

    bool RemoveManifest(int manifestIndex);

};

#endif // DM_LIVEUPDATE_H