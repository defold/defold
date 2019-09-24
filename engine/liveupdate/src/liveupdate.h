#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <resource/resource.h>
#include <resource/liveupdate_ddf.h>
#include <resource/resource_archive.h>

namespace dmLiveUpdate
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                        =  0,
        RESULT_INVALID_HEADER            = -1,
        RESULT_MEM_ERROR                 = -2,
        RESULT_INVALID_RESOURCE          = -3,
        RESULT_VERSION_MISMATCH          = -4,
        RESULT_ENGINE_VERSION_MISMATCH   = -5,
        RESULT_SIGNATURE_MISMATCH        = -6,
        RESULT_SCHEME_MISMATCH           = -7,
        RESULT_BUNDLED_RESOURCE_MISMATCH = -8,
        RESULT_FORMAT_ERROR              = -9,
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

    /**
     * Callback data from store manifest function
     */
    struct StoreManifestCallbackData
    {
        StoreManifestCallbackData()
        {
            memset(this, 0x0, sizeof(StoreManifestCallbackData));
        }
        void*       m_L;
        int         m_Self;
        int         m_Callback;
        int         m_Status;
    };

    const int MAX_MANIFEST_COUNT = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;
    const uint32_t PROJ_ID_LEN = 41; // SHA1 + NULL terminator

    void Initialize(const dmResource::HFactory factory);
    void Finalize();
    void Update();

    uint32_t GetMissingResources(const dmhash_t urlHash, char*** buffer);

    /*
     * Hashes the actual resource data with the same hashing algorithm as spec. by manifest, and compares to the expected resource hash
     */
    bool VerifyResource(dmResource::Manifest* manifest, const char* expected, uint32_t expectedLength, const dmResourceArchive::LiveUpdateResource* resource);

    /*
     * Verifies the manifest cryptographic signature and that the manifest supports the current running dmengine version.
     */
    Result VerifyManifest(dmResource::Manifest* manifest);

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(StoreResourceCallbackData*), StoreResourceCallbackData& callback_data);

    Result StoreManifest(dmResource::Manifest* manifest);

    Result ParseManifestBin(uint8_t* manifest_data, size_t manifest_len, dmResource::Manifest* manifest);

    dmResource::Manifest* GetCurrentManifest();

    char* DecryptSignatureHash(const uint8_t* pub_key_buf, uint32_t pub_key_len, uint8_t* signature, uint32_t signature_len, uint32_t* out_digest_len);
};

#endif // DM_LIVEUPDATE_H
