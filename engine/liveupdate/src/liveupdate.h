#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <resource/resource.h>
#include <resource/manifest_ddf.h>
#include <resource/resource_archive.h>

namespace dmLiveUpdate
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                        =  0,   //!< RESULT_OK
        RESULT_INVALID_HEADER            = -1,   //!< RESULT_INVALID_HEADER
        RESULT_MEM_ERROR                 = -2,   //!< RESULT_MEM_ERROR
        RESULT_INVALID_RESOURCE          = -3    //!< RESULT_INVALID_RESOURCE
    };

    /**
     * Callback data from store resource function
     */
    struct StoreResourceCallbackData
    {
        StoreResourceCallbackData()
        {
            memset(this, 0x0, sizeof(StoreResourceCallbackData));
        }
        void*       m_L;
        int         m_Self;
        int         m_Callback;
        int         m_ResourceRef;
        int         m_HexDigestRef;
        const char* m_HexDigest;
        bool        m_Status;
    };

    const int MAX_MANIFEST_COUNT = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;
    const uint32_t PROJ_ID_LEN = 41; // SHA1 + NULL terminator

    void Initialize(const dmResource::HFactory factory);
    void Finalize();
    void Update();

    uint32_t GetMissingResources(const dmhash_t urlHash, char*** buffer);

    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const dmResourceArchive::LiveUpdateResource* resource);

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(StoreResourceCallbackData*), StoreResourceCallbackData& callback_data);

    Result StoreManifest(dmResource::Manifest* manifest);

    int AddManifest(dmResource::Manifest* manifest);

    dmResource::Manifest* GetManifest(int manifestIndex);

    bool RemoveManifest(int manifestIndex);

};

#endif // DM_LIVEUPDATE_H
