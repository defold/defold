#include <stdlib.h>
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
#include "../crash_private.h"

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

// DEBUG START
// Due to a new behavior, the following test fails.
// And, since we currently don't have access to the CI machines, this is our way forward
{
    // Excerpt from our crashtool.cpp

    printf("\n%s:\n", "USERDATA");
    for( int i = 0; i < (int)dmCrash::AppState::USERDATA_SLOTS; ++i)
    {
        const char* value = dmCrash::GetUserField(d, i);
        if(!value || strcmp(value, "")==0)
        {
            break;
        }
        printf("%02d: %s\n", i, value);
    }

    printf("\n%s:\n", "BACKTRACE");

    int frames = dmCrash::GetBacktraceAddrCount(d);
    for( int i = 0; i < frames; ++i )
    {
        uintptr_t ptr = (uintptr_t)dmCrash::GetBacktraceAddr(d, i);

        const char* value = 0;//PlatformGetSymbol(info, ptr);
        printf("%02d  0x%016llX: %s\n", frames - i - 1, (unsigned long long)ptr, value ? value : "<no symbol name>");
    }

    printf("\n%s:\n", "EXTRA DATA");

    const char* extra_data = dmCrash::GetExtraData(d);
    printf("%s\n", extra_data ? extra_data : "<null>");


    printf("\n%s:\n", "MODULES");
    for( int i = 0; i < (int)dmCrash::AppState::MODULES_MAX; ++i)
    {
        const char* modulename = dmCrash::GetModuleName(d, i);
        if( modulename == 0 )
        {
            break;
        }
        uintptr_t ptr = (uintptr_t)dmCrash::GetModuleAddr(d, i);
        printf("%02d: %s  0x%016llx\n", i, modulename ? modulename : "<null>", (unsigned long long)ptr);
    }
    printf("\n");
}
// DEBUG END

    uint32_t addresses = dmCrash::GetBacktraceAddrCount(d);
    ASSERT_GT(addresses, 4);
    for (uint32_t i=0;i!=addresses;i++)
    {
        ASSERT_NE((void*)0, dmCrash::GetBacktraceAddr(d, i));
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
    ASSERT_NE(0, dmCrash::LoadPrevious());
    dmCrash::Purge();
    ASSERT_EQ(0, dmCrash::LoadPrevious());
}

TEST_F(dmCrashTest, TestPurgeDefaultPath)
{
    dmCrash::Purge();
    dmCrash::WriteDump();
    ASSERT_NE(0, dmCrash::LoadPrevious());
    dmCrash::Purge();
    ASSERT_EQ(0, dmCrash::LoadPrevious());
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
