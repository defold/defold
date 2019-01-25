#include <stdint.h>
#include <stdio.h>
#include <string>

#include <gtest/gtest.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/ssdp.h>
#include <dlib/ssdp_private.h>
#include <dlib/dstrings.h>

namespace
{

    static const char* DEVICE_DESC =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:defold=\"urn:schemas-defold-com:DEFOLD-1-0\">\n"
        "    <specVersion>\n"
        "        <major>1</major>\n"
        "        <minor>0</minor>\n"
        "    </specVersion>\n"
        "    <device>\n"
        "        <deviceType>upnp:rootdevice</deviceType>\n"
        "        <friendlyName>Defold System</friendlyName>\n"
        "        <manufacturer>Defold</manufacturer>\n"
        "        <modelName>Defold Engine 1.0</modelName>\n"
        "        <UDN>%s</UDN>\n"
        "    </device>\n"
        "</root>\n";

    void CreateRandomNumberString(char* string, unsigned int size)
    {
        for (unsigned int i = 0; i < (size - 1); ++i) {
            int ascii = '0' + (rand() % 10);
            string[i] = (char) ascii;
        }

        string[size - 1] = 0x0;
    }

    void CreateRandomUDN(char* string, unsigned int size)
    {
        char deviceId[9] = { 0 };
        CreateRandomNumberString(deviceId, 9);
        DM_SNPRINTF(string, size, "uuid:%s-3d4f-339c-8c4d-f7c6da6771c8", deviceId);
    }

    void CreateDeviceDescriptionXML(char* string, const char* udn, unsigned int size)
    {
        DM_SNPRINTF(string, size, DEVICE_DESC, udn);
    }

    void CreateDeviceDescription(dmSSDP::DeviceDesc* deviceDesc)
    {
        char* id = (char*) calloc(15, sizeof(char));
        char* desc = (char*) calloc(513, sizeof(char));

        CreateRandomNumberString(id, 15);
        CreateRandomUDN(deviceDesc->m_UDN, 43);
        CreateDeviceDescriptionXML(desc, deviceDesc->m_UDN, 513);

        deviceDesc->m_Id = id;
        deviceDesc->m_DeviceType = "upnp:rootdevice";
        deviceDesc->m_DeviceDescription = desc;
    }

    void FreeDeviceDescription(dmSSDP::DeviceDesc* deviceDesc)
    {
        free((void*) deviceDesc->m_Id);
        free((void*) deviceDesc->m_DeviceDescription);
    }


    int GetInterfaces(dmSocket::IfAddr* interfaces, uint32_t size)
    {
        int index = 0;
        uint32_t if_addr_count = 0;
        dmSocket::GetIfAddresses(interfaces, size, &if_addr_count);
        if (if_addr_count > 0)
        {
            for (int i = 0; i < size; ++i)
            {
                if (!dmSocket::Empty(interfaces[i].m_Address))
                {
                    if (interfaces[i].m_Address.m_family == dmSocket::DOMAIN_IPV4)
                    {
                        if (i > index)
                        {
                            interfaces[index] = interfaces[i];
                        }

                        index += 1;
                    }
                }
            }
        }

        return index;
    }

    dmSSDP::SSDP* CreateSSDPClient()
    {
        dmSSDP::SSDP* instance = 0x0;
        dmSSDP::NewParams params;
        dmSSDP::New(&params, &instance);

        return instance;
    }

    dmSSDP::SSDP* CreateSSDPServer(int max_age = 1800, int announce_interval = 900, bool announce = false)
    {
        dmSSDP::SSDP* instance = 0x0;
        dmSSDP::NewParams params;
        params.m_MaxAge = max_age;
        params.m_AnnounceInterval = announce_interval;
        params.m_Announce = announce ? 1 : 0;

        dmSSDP::New(&params, &instance);
        return instance;
    }

    void IterateCallbackDelete(int* context, const uint64_t* key, dmSSDP::Device** value)
    {
        dmLogDebug("Removing device %s", (*value)->m_DeviceDesc->m_Id);
        delete *value;
    }

    void DestroySSDPInstance(dmSSDP::SSDP* instance) {
        int context = 0x0;
        instance->m_RegisteredEntries.Iterate(IterateCallbackDelete, &context);
        instance->m_RegisteredEntries.Clear();
        dmSSDP::Delete(instance);
    }
};

class dmSSDPInternalTest: public ::testing::Test
{
public:

    virtual void SetUp()
    {
        dmSocket::Initialize();
    }

    virtual void TearDown()
    {
        dmSocket::Finalize();
    }

};


/* -------------------------------------------------------------------------*
 * (Internal functions) Create/Connect new SSDP sockets
 * ------------------------------------------------------------------------ */
TEST_F(dmSSDPInternalTest, NewSocket_IPv4)
{
    dmSocket::Socket instance = dmSSDP::NewSocket(dmSocket::DOMAIN_IPV4);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    dmSocket::Result actual = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, actual);
}

TEST_F(dmSSDPInternalTest, NewSocket_IPv6)
{
    dmSocket::Socket instance = dmSSDP::NewSocket(dmSocket::DOMAIN_IPV6);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    dmSocket::Result actual = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, actual);
}

TEST_F(dmSSDPInternalTest, Connect)
{
    dmSSDP::SSDP* instance = new dmSSDP::SSDP();
    dmSSDP::Result actual = dmSSDP::Connect(instance);
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);

    // Teardown
    dmSSDP::Disconnect(instance);
    delete instance;
}


/* -------------------------------------------------------------------------*
 * (Exposed function) Create/Delete new SSDP instances
 * ------------------------------------------------------------------------ */
TEST_F(dmSSDPInternalTest, New)
{
    // Setup
    dmSSDP::SSDP* instance = NULL;
    dmSSDP::NewParams params;
    dmSSDP::Result actual = dmSSDP::New(&params, &instance);

    // Test
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);
    ASSERT_EQ(1800, instance->m_MaxAge);
    ASSERT_EQ(1, instance->m_Announce);
    ASSERT_EQ(900, instance->m_AnnounceInterval);
    ASSERT_TRUE(instance->m_HttpServer != NULL);

    // Teardown
    DestroySSDPInstance(instance);
}

TEST_F(dmSSDPInternalTest, Delete)
{
    // Setup
    dmSSDP::SSDP* instance = CreateSSDPClient();

    // Test
    dmSSDP::Result actual = dmSSDP::Delete(instance);
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);
}


/* -------------------------------------------------------------------------*
 * (Exposed function) Register/Remove device for SSDP instance
 * ------------------------------------------------------------------------ */
TEST_F(dmSSDPInternalTest, RegisterDevice)
{
    // Setup
    dmSSDP::SSDP* instance = CreateSSDPClient();
    dmSSDP::DeviceDesc deviceDesc;
    CreateDeviceDescription(&deviceDesc);

    // Test
    dmSSDP::Result actual = dmSSDP::RegisterDevice(instance, &deviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);

    actual = dmSSDP::RegisterDevice(instance, &deviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_ALREADY_REGISTERED, actual);

    // Teardown
    DestroySSDPInstance(instance);
    FreeDeviceDescription(&deviceDesc);
}

TEST_F(dmSSDPInternalTest, RegisterDevice_MaximumDevices)
{
    // Setup
    dmSSDP::SSDP* instance = CreateSSDPClient();

    // Test
    const uint32_t CONST_DEVICE_COUNT = 32;
    dmSSDP::DeviceDesc deviceDescs[CONST_DEVICE_COUNT];
    for (unsigned int i = 0; i < CONST_DEVICE_COUNT; ++i)
    {
        CreateDeviceDescription(&deviceDescs[i]);
        dmSSDP::Result actual = dmSSDP::RegisterDevice(instance, &deviceDescs[i]);
        ASSERT_EQ(dmSSDP::RESULT_OK, actual);
    }

    // Test register too many devices
    dmSSDP::DeviceDesc deviceDescOverflow;
    CreateDeviceDescription(&deviceDescOverflow);
    dmSSDP::Result actual = dmSSDP::RegisterDevice(instance, &deviceDescOverflow);
    ASSERT_EQ(dmSSDP::RESULT_OUT_OF_RESOURCES, actual);
    FreeDeviceDescription(&deviceDescOverflow);

    // Teardown
    DestroySSDPInstance(instance);
    for (unsigned int i = 0; i < CONST_DEVICE_COUNT; ++i)
    {
        FreeDeviceDescription(&deviceDescs[i]);
    }
}

TEST_F(dmSSDPInternalTest, DeregisterDevice)
{
    // Setup
    dmSSDP::SSDP* instance = CreateSSDPClient();

    dmSSDP::DeviceDesc deviceDesc;
    CreateDeviceDescription(&deviceDesc);

    // Test
    dmSSDP::Result actual = dmSSDP::RegisterDevice(instance, &deviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);

    actual = dmSSDP::DeregisterDevice(instance, deviceDesc.m_Id);
    ASSERT_EQ(dmSSDP::RESULT_OK, actual);

    actual = dmSSDP::DeregisterDevice(instance, deviceDesc.m_Id);
    ASSERT_EQ(dmSSDP::RESULT_NOT_REGISTERED, actual);

    // Teardown
    DestroySSDPInstance(instance);
    FreeDeviceDescription(&deviceDesc);
}

/* -------------------------------------------------------------------------*
 * (Internal functions) Update SSDP instance
 * ------------------------------------------------------------------------ */
TEST_F(dmSSDPInternalTest, UpdateListeningSockets)
{
    // Setup
    dmSSDP::SSDP* instance = CreateSSDPClient();

    // Test
    dmSocket::IfAddr interfaces[dmSSDP::SSDP_MAX_LOCAL_ADDRESSES];
    memset(interfaces, 0, sizeof(interfaces));
    uint32_t interface_count = GetInterfaces(interfaces, dmSSDP::SSDP_MAX_LOCAL_ADDRESSES);
    ASSERT_GE(interface_count, 1) << "There are no IPv4 interface(s) available";

    dmSSDP::UpdateListeningSockets(instance, interfaces, interface_count);

    ASSERT_EQ(interface_count, instance->m_LocalAddrCount);

    for (unsigned int i = 0; i < interface_count; ++i)
    {
        ASSERT_EQ(interfaces[i].m_Address, instance->m_LocalAddr[i].m_Address)
            << "An interface has been ignored";
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance->m_LocalAddrSocket[i])
            << "An interface has an invalid socket handle";
    }

    // Teardown
    DestroySSDPInstance(instance);
}

TEST_F(dmSSDPInternalTest, SendAnnounce)
{
    // Setup
    dmSSDP::Result result = dmSSDP::RESULT_OK;
    dmSSDP::SSDP* instance = CreateSSDPClient();

    dmSSDP::DeviceDesc deviceDesc;
    CreateDeviceDescription(&deviceDesc);
    result = dmSSDP::RegisterDevice(instance, &deviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, result);
    dmSSDP::Device** device = instance->m_RegisteredEntries.Get(dmHashString64(deviceDesc.m_Id));

    dmSocket::IfAddr interfaces[dmSSDP::SSDP_MAX_LOCAL_ADDRESSES];
    memset(interfaces, 0, sizeof(interfaces));
    uint32_t interface_count = GetInterfaces(interfaces, dmSSDP::SSDP_MAX_LOCAL_ADDRESSES);
    dmSSDP::UpdateListeningSockets(instance, interfaces, interface_count);

    // Test
    ASSERT_TRUE(dmSSDP::SendAnnounce(instance, *device, 0));

    // Teardown
    DestroySSDPInstance(instance);
    FreeDeviceDescription(&deviceDesc);
}

TEST_F(dmSSDPInternalTest, SendUnannounce)
{
    // Setup
    dmSSDP::Result result = dmSSDP::RESULT_OK;
    dmSSDP::SSDP* instance = CreateSSDPClient();

    dmSSDP::DeviceDesc deviceDesc;
    CreateDeviceDescription(&deviceDesc);
    result = dmSSDP::RegisterDevice(instance, &deviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, result);
    dmSSDP::Device** device = instance->m_RegisteredEntries.Get(dmHashString64(deviceDesc.m_Id));

    dmSocket::IfAddr interfaces[dmSSDP::SSDP_MAX_LOCAL_ADDRESSES];
    memset(interfaces, 0, sizeof(interfaces));
    uint32_t interface_count = GetInterfaces(interfaces, dmSSDP::SSDP_MAX_LOCAL_ADDRESSES);
    dmSSDP::UpdateListeningSockets(instance, interfaces, interface_count);

    // Test
    ASSERT_NO_FATAL_FAILURE(dmSSDP::SendUnannounce(instance, *device, 0));

    // Teardown
    DestroySSDPInstance(instance);
    FreeDeviceDescription(&deviceDesc);
}

TEST_F(dmSSDPInternalTest, ClientServer_MatchingInterfaces)
{
    // Setup
    dmSSDP::SSDP* client = CreateSSDPClient();
    dmSSDP::SSDP* server = CreateSSDPServer();

    // Test
    dmSSDP::Update(server, false);
    dmSSDP::Update(client, false);

    ASSERT_GE(server->m_LocalAddrCount, 1);
    ASSERT_EQ(server->m_LocalAddrCount, client->m_LocalAddrCount);

    for (unsigned int i = 0; i < server->m_LocalAddrCount; ++i)
    {
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, client->m_LocalAddrSocket[i]);
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, server->m_LocalAddrSocket[i]);
        ASSERT_EQ(server->m_LocalAddr[i].m_Address, client->m_LocalAddr[i].m_Address);
    }

    // Teardown
    dmSSDP::Delete(server);
    DestroySSDPInstance(client);
}

int main(int argc, char **argv)
{
    srand(time(NULL));
    dmLogSetlevel(DM_LOG_SEVERITY_DEBUG);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
