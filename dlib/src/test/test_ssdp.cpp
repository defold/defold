#include <stdint.h>
#include <stdio.h>
#include <string>
#include <map>
#include <gtest/gtest.h>
#include "../dlib/ssdp.h"
#include "../dlib/time.h"
#include "../dlib/log.h"
#include "../dlib/dstrings.h"
#include "../dlib/hash.h"
#include "../dlib/socket.h"

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
"        <UDN>${UDN}</UDN>\n"
"    </device>\n"
"</root>\n";

static const char* TEST_UDN = "uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8";
static const char* TEST_USN = "uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";
static dmhash_t TEST_USN_HASH = dmHashString64(TEST_USN);

void InitDeviceDesc(dmSSDP::DeviceDesc& device_desc)
{
    device_desc.m_Id = "my_root_device";
    device_desc.m_DeviceDescription = DEVICE_DESC;
    device_desc.m_DeviceType = "upnp:rootdevice";
    dmStrlCpy(device_desc.m_UDN, TEST_UDN, sizeof(device_desc.m_UDN));
}

class dmSSDPTest: public ::testing::Test
{
public:
    std::map<dmhash_t, dmSSDP::HDevice> m_ServerDevices;
    std::map<dmhash_t, dmSSDP::HDevice> m_ClientDevices;

    static void DeviceCallback(std::map<dmhash_t, dmSSDP::HDevice>* devices, const dmhash_t* usn, const dmSSDP::HDevice device)
    {
        (*devices)[*usn] = device;
    }

    dmSSDP::HSSDP m_Server;
    dmSSDP::HSSDP m_Client;
    dmSSDP::DeviceDesc m_DeviceDesc;

    bool TestDeviceDiscovered()
    {
        return m_ClientDevices.find(TEST_USN_HASH) != m_ClientDevices.end();
    }

    void UpdateClient(bool search = false)
    {
        dmSSDP::Update(m_Client, search);
        Refresh();
    }

    void UpdateServer(bool search = false)
    {
        dmSSDP::Update(m_Server, search);
        Refresh();
    }

    void WaitPackage()
    {
        dmTime::Sleep(50 * 1000);
    }

    void Refresh()
    {
        m_ServerDevices.clear();
        m_ClientDevices.clear();
        if (m_Server)
            dmSSDP::IterateDevices(m_Server, DeviceCallback, &m_ServerDevices);
        if (m_Client)
            dmSSDP::IterateDevices(m_Client, DeviceCallback, &m_ClientDevices);
    }

    void Init(int max_age = -1, bool announce = true)
    {
        dmSSDP::Result r;

        dmSSDP::NewParams client_params;
        r = dmSSDP::New(&client_params, &m_Client);
        ASSERT_EQ(dmSSDP::RESULT_OK, r);

        dmSSDP::NewParams server_params;
        if (max_age != -1)
            server_params.m_MaxAge = max_age;
        server_params.m_Announce = announce;
        r = dmSSDP::New(&server_params, &m_Server);
        ASSERT_EQ(dmSSDP::RESULT_OK, r);
        InitDeviceDesc(m_DeviceDesc);
        r = dmSSDP::RegisterDevice(m_Server, &m_DeviceDesc);
        ASSERT_EQ(dmSSDP::RESULT_OK, r);

        // Dispatch announce messages
        UpdateServer();
        WaitPackage();
        UpdateClient();
    }

    virtual void SetUp()
    {
        m_Client = 0;
        m_Server = 0;
    }

    virtual void TearDown()
    {
        dmSSDP::Result r;

        if (m_Server)
        {
            r = dmSSDP::DeregisterDevice(m_Server, "my_root_device");
            ASSERT_EQ(dmSSDP::RESULT_OK, r);
            r = dmSSDP::Delete(m_Server);
            ASSERT_EQ(dmSSDP::RESULT_OK, r);
        }
        if (m_Client)
        {
            r = dmSSDP::Delete(m_Client);
            ASSERT_EQ(dmSSDP::RESULT_OK, r);
        }
    }
};

TEST(dmSSDP, RegisterDevice)
{
    dmSSDP::HSSDP ssdp;

    dmSSDP::DeviceDesc device_desc;
    InitDeviceDesc(device_desc);

    dmSSDP::NewParams params;
    dmSSDP::Result r = dmSSDP::New(&params, &ssdp);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &device_desc);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &device_desc);
    ASSERT_EQ(dmSSDP::RESULT_ALREADY_REGISTRED, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &device_desc);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_NOT_REGISTRED, r);

    r = dmSSDP::Delete(ssdp);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
}

TEST_F(dmSSDPTest, Search)
{
    // To search for devices we must invoke client -> server -> client
    // 1. Send request (client)
    // 2. Process and respond to request (server)
    // 3. Handle response (client)

    Init();

    dmSSDP::ClearDiscovered(m_Client);
    Refresh();
    ASSERT_FALSE(TestDeviceDiscovered());

    UpdateClient(true);
    ASSERT_FALSE(TestDeviceDiscovered());

    UpdateServer();
    ASSERT_FALSE(TestDeviceDiscovered());

    WaitPackage();
    UpdateClient();
    ASSERT_TRUE(TestDeviceDiscovered());
}

TEST_F(dmSSDPTest, Announce)
{
    Init();
    ASSERT_TRUE(TestDeviceDiscovered());
}

TEST_F(dmSSDPTest, Unannounce)
{
    Init();
    ASSERT_TRUE(TestDeviceDiscovered());

    dmSSDP::Result r;
    r = dmSSDP::DeregisterDevice(m_Server, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
    r = dmSSDP::Delete(m_Server);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
    m_Server = 0;

    WaitPackage();
    UpdateClient();

    ASSERT_FALSE(TestDeviceDiscovered());
}

TEST_F(dmSSDPTest, Expire)
{
    Init(1, false);
    dmTime::Sleep(1000000);
    UpdateServer();
    UpdateClient();
    ASSERT_FALSE(TestDeviceDiscovered());
}

TEST_F(dmSSDPTest, Renew)
{
    Init(1);
    dmTime::Sleep(1000000);
    UpdateServer();
    UpdateClient();
    ASSERT_TRUE(TestDeviceDiscovered());
}

int main(int argc, char **argv)
{
    dmLogSetlevel(DM_LOG_SEVERITY_INFO);
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

