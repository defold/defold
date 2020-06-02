// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdlib.h>
#include <time.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
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

#if defined(__NX__)
    #define MOUNTFS "host:/"
#else
    #define MOUNTFS ""
#endif

class dmCrashTest : public jc_test_base_class
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

#if !defined(__NX__)
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
    ASSERT_GE(addresses, 2u);
    for (uint32_t i=0;i!=addresses;i++)
    {
        // DEF-3128: Skip the last one, since it might be 0 on Win32
        if (i != addresses-1 )
        {
            ASSERT_NE((void*)0, dmCrash::GetBacktraceAddr(d, i));
        }
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
    dmCrash::SetFilePath(MOUNTFS "remove-me");
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
#endif


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
