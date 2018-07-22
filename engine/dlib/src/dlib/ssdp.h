#ifndef DM_SSDP
#define DM_SSDP

#include <string.h>
#include <dlib/hash.h>

/**
 * Simple Service Discovery Protocol
 */
namespace dmSSDP
{
    /**
     * SSDP handle
     */
    typedef struct SSDP* HSSDP;

    /**
     * Device handle
     */
    typedef struct Device* HDevice;

    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK = 0,                //!< RESULT_OK
        RESULT_NETWORK_ERROR = -1,    //!< RESULT_NETWORK_ERROR
        RESULT_ALREADY_REGISTERED = -2,//!< RESULT_ALREADY_REGISTERED
        RESULT_NOT_REGISTERED = -3,    //!< RESULT_NOT_REGISTERED
        RESULT_OUT_OF_RESOURCES = -4, //!< RESULT_OUT_OF_RESOURCES
    };

    /**
     * Device description
     */
    struct DeviceDesc
    {
        /// Unique identifier (unique in this context).
        /// Not part of UPnP specification but used in URL:s for this device
        const char* m_Id;

        /// UPnP device type
        const char* m_DeviceType;

        /// Device description template in XML form.
        /// This can include the variable ${HTTP-HOST} in the url
        const char* m_DeviceDescription;

        /// Unique Device Name, eg uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8
        /// Should include the uuid:-prefix
        char        m_UDN[128];
    };

    /**
     * New SDDP params
     */
    struct NewParams
    {
        NewParams()
        {
            memset(this, 0, sizeof(*this));
            m_MaxAge = 1800;
            m_Announce = 1;
            m_AnnounceInterval = 900;
        }

        /// Time an advertisement is valid
        uint32_t m_MaxAge;

        /// Frequency of sending announcement messages. This is typically m_MaxAge / 2
        uint32_t m_AnnounceInterval;

        /// Announce registered devices. Default is true
        uint32_t m_Announce : 1;
    };

    /**
     * Create a new SSDP
     * @param params params
     * @param ssdp out handle
     * @return RESULT_OK on success
     */
    Result New(const NewParams* params, HSSDP* ssdp);

    /**
     * Delete SSDDP
     * @param ssdp ssdp to delete
     * @return RESULT_OK on success
     */
    Result Delete(HSSDP ssdp);

    /**
     * Register a new device
     * @note The device description pointer must valid throughout the life-time
     * of the registered device.
     * @param ssdp ssdp handle
     * @param device_desc device description
     * @return RESULT_OK on success
     */
    Result RegisterDevice(HSSDP ssdp, const DeviceDesc* device_desc);

    /**
     * Deregister device
     * @param ssdp ssdp handle
     * @param id id
     * @return RESULT_OK on success
     */
    Result DeregisterDevice(HSSDP ssdp, const char* id);

    /**
     * Update
     * @param ssdp ssdp handle
     * @param search true for periodical search to discover other devices
     */
    void Update(HSSDP ssdp, bool search);

    /**
     * Remove all discovered devics
     * @param ssdp ssdp handle
     */
    void ClearDiscovered(HSSDP ssdp);

    void IterateDevicesInternal(HSSDP ssdp, void (*call_back)(void *context, const dmhash_t* usn, Device* device), void* context);

    /**
     * Iterate through all discovered devices
     * @param ssdp ssdp handle
     * @param call_back function to be called for every device
     * @param context user context
     */
    template <typename CONTEXT>
    void IterateDevices(HSSDP ssdp, void (*call_back)(CONTEXT *context, const dmhash_t* usn, const HDevice device), CONTEXT* context)
    {
        IterateDevicesInternal(ssdp, (void (*)(void *, const dmhash_t*, Device*)) call_back, (void*) context);
    }

}  // namespace dmSSDP


#endif // DM_SSDP
