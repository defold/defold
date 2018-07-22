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

static const char* DEVICE_DESC_STATIC =
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
    "        <UDN>uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8</UDN>\n"
    "    </device>\n"
    "</root>\n";

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

void WaitPackage()
{
    dmTime::Sleep(50 * 1000);
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

    char* m_DeviceUDN;
    char* m_DeviceUSN;
    char* m_DeviceDescriptionXML;
    dmhash_t m_DeviceUSNHash;
    dmSSDP::DeviceDesc m_DeviceDesc;

    bool TestDeviceDiscovered()
    {
        return m_ClientDevices.find(m_DeviceUSNHash) != m_ClientDevices.end();
    }

    void UpdateClient(bool search = false)
    {
        dmSSDP::Update(m_Client, search);
        Refresh();
    }

    void UpdateServer()
    {
        dmSSDP::Update(m_Server, false);
        Refresh();
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
        {
            server_params.m_MaxAge = max_age;
            server_params.m_AnnounceInterval = max_age;
        }
        server_params.m_Announce = announce;
        r = dmSSDP::New(&server_params, &m_Server);
        ASSERT_EQ(dmSSDP::RESULT_OK, r);
        r = dmSSDP::RegisterDevice(m_Server, &m_DeviceDesc);
        ASSERT_EQ(dmSSDP::RESULT_OK, r);

        // Dispatch announce messages
        UpdateServer();
        WaitPackage();
        UpdateClient();
    }

    virtual void SetUp()
    {
        m_DeviceUDN = (char*) calloc(43, sizeof(char));
        m_DeviceUSN = (char*) calloc(60, sizeof(char));
        m_DeviceDescriptionXML = (char*) calloc(513, sizeof(char));

        CreateRandomUDN(m_DeviceUDN, 43);

        dmStrlCat(m_DeviceUSN, m_DeviceUDN, 60);
        dmStrlCat(m_DeviceUSN, "::upnp:rootdevice", 60);

        m_DeviceUSNHash = dmHashString64(m_DeviceUSN);

        CreateDeviceDescriptionXML(m_DeviceDescriptionXML, m_DeviceUDN, 513);

        printf("(SETUP) Using UDN = \"%s\"\n", m_DeviceUDN);
        printf("(SETUP) Using USN = \"%s\"\n", m_DeviceUSN);
        printf("(SETUP) Using USN_HASH = \"%llu\"\n", (unsigned long long)m_DeviceUSNHash);

        m_DeviceDesc.m_Id = "my_root_device";
        m_DeviceDesc.m_DeviceDescription = m_DeviceDescriptionXML;
        m_DeviceDesc.m_DeviceType = "upnp:rootdevice";
        dmStrlCpy(m_DeviceDesc.m_UDN, m_DeviceUDN, sizeof(m_DeviceDesc.m_UDN));

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

        m_DeviceUSNHash = 0;
        free(m_DeviceUDN);
        free(m_DeviceUSN);
        free(m_DeviceDescriptionXML);
    }
};

TEST_F(dmSSDPTest, RegisterDevice)
{
    dmSSDP::HSSDP ssdp;

    dmSSDP::NewParams params;
    dmSSDP::Result r = dmSSDP::New(&params, &ssdp);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &m_DeviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &m_DeviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_ALREADY_REGISTERED, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::RegisterDevice(ssdp, &m_DeviceDesc);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::DeregisterDevice(ssdp, "my_root_device");
    ASSERT_EQ(dmSSDP::RESULT_NOT_REGISTERED, r);

    r = dmSSDP::Delete(ssdp);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
}

#ifndef _WIN32

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

    // NOTE: We used to have a single iteration here
    // but the tests failed occasionally on Mac
    // It could be related to lost packages even though
    // it might be farfetched that packages are lost on the local network.
    // Another possible explanation is interference with UPnP devices on the network, i.e. router.
    // It could of course also be a bug in the SSDP implementation
    bool discovered = false;
    int iter = 0;
    while (!discovered && iter++ < 10) {
        UpdateServer();
        WaitPackage();
        UpdateClient();
        discovered = TestDeviceDiscovered();
    }

    ASSERT_TRUE(discovered);
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

#endif

TEST_F(dmSSDPTest, Expire)
{
    Init(1, false);
    dmTime::Sleep(1000000);
    UpdateServer();
    UpdateClient();
    ASSERT_FALSE(TestDeviceDiscovered());
}

#ifndef _WIN32

TEST_F(dmSSDPTest, Renew)
{
    Init(1);
    dmTime::Sleep(1000000);
    UpdateServer();
    WaitPackage();
    UpdateClient();
    ASSERT_TRUE(TestDeviceDiscovered());
}

#endif

#ifndef _WIN32
#include <sys/select.h>

/*
 * TODO:
 * select() on file descriptors is not available on windows
 * Port this test to use WaitForSingleObject or other windows appropriate stuff
 */

TEST_F(dmSSDPTest, JavaClient)
{
    /*
     * About the servers:
     * Server1: max-age=1, announce=true
     * Server2: max-age=2, announce=false
     *
     * Server1 is used to test announce messages
     * Server2 is used to test expiration (client-side)
     * Server2 is also used for search as automatic announcement is disabled
     *
     */
    dmSSDP::HSSDP server1, server2;
    dmSSDP::DeviceDesc device_desc1, device_desc2;

    char device1_udn[43] = { 0 };
    char device1_usn[60] = { 0 };
    CreateRandomUDN(device1_udn, 43);
    dmStrlCat(device1_usn, device1_udn, 60);
    dmStrlCat(device1_usn, "::upnp:rootdevice", 60);

    char device2_udn[43] = { 0 };
    char device2_usn[60] = { 0 };
    CreateRandomUDN(device2_udn, 43);
    dmStrlCat(device2_usn, device2_udn, 60);
    dmStrlCat(device2_usn, "::upnp:rootdevice", 60);

    dmSSDP::Result r;
    dmSSDP::NewParams server_params1;
    server_params1.m_MaxAge = 1;
    server_params1.m_AnnounceInterval = 1;
    server_params1.m_Announce = 1;
    r = dmSSDP::New(&server_params1, &server1);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
    device_desc1.m_Id = "my_root_device1";
    device_desc1.m_DeviceDescription = DEVICE_DESC_STATIC;
    device_desc1.m_DeviceType = "upnp:rootdevice";
    dmStrlCpy(device_desc1.m_UDN, device1_udn, sizeof(device_desc1.m_UDN));
    r = dmSSDP::RegisterDevice(server1, &device_desc1);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    dmSSDP::NewParams server_params2;
    server_params2.m_MaxAge = 2;
    server_params2.m_AnnounceInterval = 2;
    server_params2.m_Announce = 0;
    r = dmSSDP::New(&server_params2, &server2);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
    device_desc2.m_Id = "my_root_device2";
    device_desc2.m_DeviceDescription = DEVICE_DESC_STATIC;
    device_desc2.m_DeviceType = "upnp:rootdevice";
    dmStrlCpy(device_desc2.m_UDN, device2_udn, sizeof(device_desc2.m_UDN));
    r = dmSSDP::RegisterDevice(server2, &device_desc2);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    dmSSDP::Update(server1, false);
    dmSSDP::Update(server2, false);
    WaitPackage();

    printf("(C++) Sending USN1 = \"%s\"\n", device1_usn);
    printf("(C++) Sending USN2 = \"%s\"\n", device2_usn);
    char* dynamo_home = getenv("DYNAMO_HOME");
    char command[2048];
    DM_SNPRINTF(command, sizeof(command),
            "java -cp build/default/src/java:build/default/src/java_test:%s/ext/share/java/junit-4.6.jar -DUSN1=%s -DUSN2=%s org.junit.runner.JUnitCore com.dynamo.upnp.SSDPTest", dynamo_home, device1_usn, device2_usn);

    const char* mode = "r";
    FILE* f = popen(command, mode);
    fd_set set;
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 5 * 1000;
    int fd = fileno(f);

    while (true) {
        FD_ZERO(&set);
        FD_SET(fd, &set);
        int n = select(fd + 1, &set, 0, 0, &timeout);
        if (n > 0) {
            int c = fgetc(f);
            if (c == EOF)
                break;
            fputc(c, stderr);
        }

        dmSSDP::Update(server1, false);
        dmSSDP::Update(server2, false);
    }
    int ret = pclose(f);
    ASSERT_EQ(0, ret);

    r = dmSSDP::DeregisterDevice(server1, "my_root_device1");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::DeregisterDevice(server2, "my_root_device2");
    ASSERT_EQ(dmSSDP::RESULT_OK, r);

    r = dmSSDP::Delete(server1);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
    r = dmSSDP::Delete(server2);
    ASSERT_EQ(dmSSDP::RESULT_OK, r);
}

#endif

int main(int argc, char **argv)
{
    srand(time(NULL));
    dmLogSetlevel(DM_LOG_SEVERITY_DEBUG);
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

