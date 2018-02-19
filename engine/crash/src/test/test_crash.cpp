#include <stdlib.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/sys.h>
#include <script/script.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#include "../crash.h"

class dmCrashTest : public ::testing::Test
{
    public:

        virtual void SetUp()
        {

            time_t t = time(NULL);
            struct tm* tm = localtime(&t);

            m_EngineHash[0] = 0;
            strftime(m_EngineHash, sizeof(m_EngineHash), "%Y-%m-%d %H:%M:%S", tm);

            dmCrash::Init("TEST", m_EngineHash);
        }

        virtual void TearDown()
        {

        }
        char m_EngineHash[40];
};

TEST_F(dmCrashTest, Initialize)
{

}

TEST_F(dmCrashTest, TestLoad)
{
    dmCrash::WriteDump();

    dmCrash::HDump d = dmCrash::LoadPrevious();
    ASSERT_NE(d, 0);

    dmSys::SystemInfo info;
    dmSys::GetSystemInfo(&info);

    ASSERT_STREQ(m_EngineHash, dmCrash::GetSysField(d, dmCrash::SYSFIELD_ENGINE_HASH));
    ASSERT_EQ(0, strcmp("TEST", dmCrash::GetSysField(d, dmCrash::SYSFIELD_ENGINE_VERSION)));
    ASSERT_EQ(0, strcmp(info.m_DeviceModel, dmCrash::GetSysField(d, dmCrash::SYSFIELD_DEVICE_MODEL)));
    ASSERT_EQ(0, strcmp(info.m_Manufacturer, dmCrash::GetSysField(d, dmCrash::SYSFIELD_MANUFACTURER)));
    ASSERT_EQ(0, strcmp(info.m_SystemName, dmCrash::GetSysField(d, dmCrash::SYSFIELD_SYSTEM_NAME)));
    ASSERT_EQ(0, strcmp(info.m_SystemVersion, dmCrash::GetSysField(d, dmCrash::SYSFIELD_SYSTEM_VERSION)));
    ASSERT_EQ(0, strcmp(info.m_Language, dmCrash::GetSysField(d, dmCrash::SYSFIELD_LANGUAGE)));
    ASSERT_EQ(0, strcmp(info.m_DeviceLanguage, dmCrash::GetSysField(d, dmCrash::SYSFIELD_DEVICE_LANGUAGE)));
    ASSERT_EQ(0, strcmp(info.m_Territory, dmCrash::GetSysField(d, dmCrash::SYSFIELD_TERRITORY)));

    uint32_t addresses = dmCrash::GetBacktraceAddrCount(d);
    ASSERT_GT(addresses, 4);
    printf("addresses: %u\n", addresses);

    for (uint32_t i=0;i!=addresses;i++)
    {
        void* addr = dmCrash::GetBacktraceAddr(d, i);
        printf("%u: %p\n", i, addr);
        ASSERT_NE((void*)0, addr);
    }

    char buf[4096];

    int count = 0;
    for (uint32_t i=0;true;i++)
    {
        const char *name = dmCrash::GetModuleName(d, i);
        void *addr = dmCrash::GetModuleAddr(d, i);
        if (!name)
        {
            break;
        }

        ASSERT_NE((void*) 0, addr);

        // do this just to catch any misbehaving
        strcpy(buf, name);
        count++;
    }

    ASSERT_GT(count, 3);
}

TEST_F(dmCrashTest, TestPurgeCustomPath)
{
    dmCrash::SetFilePath("remove-me");
    dmCrash::Purge();
    dmCrash::WriteDump();
    ASSERT_NE(dmCrash::LoadPrevious(), 0);
    dmCrash::Purge();
    ASSERT_EQ(dmCrash::LoadPrevious(), 0);
}

TEST_F(dmCrashTest, TestPurgeDefaultPath)
{
    dmCrash::Purge();
    dmCrash::WriteDump();
    ASSERT_NE(dmCrash::LoadPrevious(), 0);
    dmCrash::Purge();
    ASSERT_EQ(dmCrash::LoadPrevious(), 0);
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
