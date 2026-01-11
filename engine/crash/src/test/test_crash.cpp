// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

        void SetUp() override
        {

            time_t t = time(NULL);
            struct tm* tm = localtime(&t);

            m_EngineHash[0] = 0;
            strftime(m_EngineHash, sizeof(m_EngineHash), "%Y-%m-%d %H:%M:%S", tm);

            dmCrash::Init("TEST", m_EngineHash);
        }

        void TearDown() override
        {
            dmCrash::Purge();
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

/*
//Not supported by jc_test.h yet (uses jc_test_use_signal_handlers())
static void VerifyCallstackSize(dmCrash::HDump d, uint32_t expected_count)
{
    uint32_t addresses = dmCrash::GetBacktraceAddrCount(d);

    const char* data = dmCrash::GetExtraData(d);
    printf("CALLSTACK:\n%s\n", data?data:"<null>");//////////

    ASSERT_GE(addresses, expected_count);
    for (uint32_t i=0;i!=addresses;i++)
    {
        if (i != addresses-1 )
        {
            ASSERT_NE((void*)0, dmCrash::GetBacktraceAddr(d, i));
        }
    }
}

static void VerifyCallstackString(dmCrash::HDump d, const char* str)
{
    const char* data = dmCrash::GetExtraData(d);
    const char* result = data != 0 ? strstr(data, str) : 0;
    ASSERT_STREQ(str, result);
}

TEST_F(dmCrashTest, TestSIGABRT)
{
    jc_test_use_signal_handlers(1);

    ASSERT_DEATH(assert(false && "Test abort"),"");

    jc_test_use_signal_handlers(0);

    dmCrash::HDump d = dmCrash::LoadPrevious();
    VerifyCallstackSize(d, 11);
    VerifyCallstackString(d, "assert.cpp");
}


void TestSegv()
{
    int* p = 0;
    *p = 17;
    printf("segv: %d\n", *p); // to not allow it to be optimized out
}

TEST_F(dmCrashTest, TestSIGSEGV) // using the signal handler, not the exception handler
{
    dmCrash::Purge();
    jc_test_use_signal_handlers(1);

    ASSERT_DEATH(TestSegv(),"");

    jc_test_use_signal_handlers(0);

    dmCrash::HDump d = dmCrash::LoadPrevious();
    ASSERT_NE(d, 0);
    VerifyCallstackSize(d, 8);
    VerifyCallstackString(d, "TestSegv");
}
*/

#endif


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
