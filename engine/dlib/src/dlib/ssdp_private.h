#ifndef DM_SSDP_PRIVATE
#define DM_SSDP_PRIVATE

#include "array.h"
#include "hashtable.h"
#include "http_client_private.h"
#include "http_server.h"
#include "http_server_private.h"
#include "template.h"
#include "time.h"

namespace dmSSDP
{

    static const uint32_t SSDP_LOCAL_ADDRESS_EXPIRATION    = 4U;
    static const uint32_t SSDP_MAX_LOCAL_ADDRESSES         = 32U;
    static const uint16_t SSDP_MCAST_PORT                  = 1900U;
    static const uint32_t SSDP_MCAST_TTL                   = 4U;

    struct Device
    {
        // Only available for registered devices
        const DeviceDesc*   m_DeviceDesc;

        struct IfAddrState {
            uint64_t m_Expires;
            dmSocket::Address m_Address;
        } m_IfAddrState[SSDP_MAX_LOCAL_ADDRESSES];

        uint32_t m_IfAddrStateCount;

        // Time when the device expires
        // For registered devices: a notification should be sent
        // For discovered devices: the device should be removed
        uint64_t            m_Expires;

        Device()
        {
            memset(this, 0, sizeof(*this));
        }

        Device(const DeviceDesc* device_desc)
        {
            memset(this, 0, sizeof(*this));
            m_DeviceDesc = device_desc;
            // NOTE: We set expires to such that announce messages
            // will be sent in one second (if enabled)
            m_Expires = dmTime::GetTime();
        }
    };

    struct SSDP
    {
        SSDP()
        {
            memset(this, 0, sizeof(*this));
            m_DiscoveredDevices.SetCapacity(983, 1024);
            m_RegistredEntries.SetCapacity(17, 32);
            m_MCastSocket = dmSocket::INVALID_SOCKET_HANDLE;
        }

        // Max age for registered devices
        uint32_t                m_MaxAge;
        char                    m_MaxAgeText[16];

        // True if announce messages should be sent
        uint32_t                m_Announce : 1;

        // Time interval for annouce messages (if m_Announce is set). Typically m_MaxAge / 2
        uint32_t                m_AnnounceInterval;

        // True if reconnection should be performed in next update
        uint32_t                m_Reconnect : 1;

        // Send/Receive buffer
        uint8_t                 m_Buffer[1500];

        // All discovered devices
        dmHashTable64<Device>   m_DiscoveredDevices;

        // All registered devices
        dmHashTable64<Device*>  m_RegistredEntries;

        // Port for m_Socket
        uint16_t                m_Port;

        // Socket for multicast receive
        dmSocket::Socket        m_MCastSocket;

        // Addresses & sockets for each available network address.
        // Mutex lock is held during Update() calls and from addr. update thread.
        dmSocket::IfAddr        m_LocalAddr[SSDP_MAX_LOCAL_ADDRESSES];
        dmSocket::Socket        m_LocalAddrSocket[SSDP_MAX_LOCAL_ADDRESSES];
        uint32_t                m_LocalAddrCount;
        uint64_t                m_LocalAddrExpires;

        // Hostname for the current request being processed.
        char                    m_HttpHost[64];

        // Http server for device descriptions
        dmHttpServer::HServer   m_HttpServer;
        char                    m_HttpPortText[8];
    };

    struct Replacer
    {
        Replacer*                   m_Parent;
        void*                       m_Userdata;
        dmTemplate::ReplaceCallback m_Callback;

        Replacer(Replacer* parent, void* user_data, dmTemplate::ReplaceCallback call_back)
        {
            m_Parent = parent;
            m_Userdata = user_data;
            m_Callback = call_back;
        }

        static const char* Replace(void* user_data, const char* key);
    };

    struct ReplaceContext
    {
        SSDP*   m_SSDP;
        Device* m_Device;

        ReplaceContext(SSDP* ssdp, Device* device)
        {
            m_SSDP = ssdp;
            m_Device = device;
        }
    };

    enum RequestType
    {
        RT_UNKNOWN    = 0,
        RT_NOTIFY     = 1,
        RT_M_SEARCH   = 2,
    };

    struct RequestParseState
    {
        SSDP*                       m_SSDP;
        // Parsed max-age
        uint32_t                    m_MaxAge;

        // Request-type, ie NOTIFY or M-SEARCH
        RequestType                 m_RequestType;

        // All headers
        dmHashTable64<const char*>  m_Headers;

        // HTTP status
        int                         m_Status;

        // Notification Type (NT)
        dmhash_t                    m_NTHash;
        // Notification Sub Type (NTS)
        dmhash_t                    m_NTSHash;

        RequestParseState(SSDP* ssdp)
        {
            memset(this, 0, sizeof(*this));
            m_SSDP = ssdp;
            m_Headers.SetCapacity(27, 64);
            // Default max-age if none is found
            m_MaxAge = 1800;
        }

        static void FreeCallback(RequestParseState *state, const dmhash_t* key, const char** value);

        ~RequestParseState()
        {
            m_Headers.Iterate(FreeCallback, this);
        }
    };

    struct SearchResponseContext
    {
        RequestParseState*  m_State;
        const char*         m_ST;
        dmSocket::Address   m_FromAddress;
        uint16_t            m_FromPort;

        SearchResponseContext(RequestParseState* state, const char* st, dmSocket::Address from_address, uint16_t from_port)
        {
            m_State = state;
            m_ST = st;
            m_FromAddress = from_address;
            m_FromPort = from_port;
        }
    };

    struct ExpireContext
    {
        SSDP*                   m_SSDP;
        uint64_t                m_Now;
        dmArray<dmhash_t>       m_ToExpire;

        ExpireContext(SSDP* ssdp)
        {
            memset(this, 0, sizeof(*this));
            m_SSDP = ssdp;
            m_Now = dmTime::GetTime();
        }
    };

    const char* ReplaceIfAddrVar(void *user_data, const char *key);
    const char* ReplaceHttpHostVar(void *user_data, const char *key);
    const char* ReplaceSSDPVar(void* user_data, const char* key);
    const char* ReplaceDeviceVar(void* user_data, const char* key);
    const char* ReplaceSearchResponseVar(void* user_data, const char* key);
    dmSocket::Socket NewSocket(dmSocket::Domain domain);
    bool AddressSortPred(dmSocket::IfAddr const &a, dmSocket::IfAddr const &b);
    void DestroyListeningSocket(SSDP *ssdp, uint32_t slot);
    void UpdateListeningSockets(SSDP *ssdp, dmSocket::IfAddr *if_addr, uint32_t if_addr_count);
    void HttpHeader(void* user_data, const char* key, const char* value);
    void HttpResponse(void* user_data, const dmHttpServer::Request* request);
    void Disconnect(SSDP* ssdp);
    Result Connect(SSDP* ssdp);
    bool SendAnnounce(HSSDP ssdp, Device* device, uint32_t iface);
    void SendUnannounce(HSSDP ssdp, Device* device, uint32_t iface);
    void VersionCallback(void* user_data, int major, int minor, int status, const char* status_str);
    void RequestCallback(void* user_data, const char* request_method, const char* resource, int major, int minor);
    void HeaderCallback(void* user_data, const char* orig_key, const char* value);
    void ResponseCallback(void* user_data, int offset);
    void HandleAnnounce(RequestParseState* state, const char* usn);
    void HandleUnAnnounce(RequestParseState* state, const char* usn);
    void SearchCallback(SearchResponseContext* ctx, const dmhash_t* key, Device** device);
    void HandleSearch(RequestParseState* state, dmSocket::Address from_address, uint16_t from_port);
    bool DispatchSocket(SSDP* ssdp, dmSocket::Socket socket, bool response);
    void VisitDiscoveredExpireDevice(ExpireContext* context, const dmhash_t* key, Device* device);
    void ExpireDiscovered(SSDP* ssdp);
    void VisitRegistredAnnounceDevice(SSDP* ssdp, const dmhash_t* key, Device** device);
    void AnnounceRegistred(SSDP* ssdp);

};

#endif