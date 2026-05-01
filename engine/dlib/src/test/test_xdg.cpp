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

#include <stdint.h>
#include <stdlib.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/dstrings.h>
#include <dlib/sys.h>
#include <dlib/sys_internal.h>
#include <dlib/path.h>
#include <dlib/log.h>
#include <dlib/testutil.h>

#if defined(__linux__)

struct dmSysXDG : public jc_test_base_class {
    const char* originalXDG = dmSys::GetEnv("XDG_DATA_HOME");
    const char* originalHome = dmSys::GetEnv("HOME");
    char testHome[128];
    char testXDG[128];
    void SetUp()
    {
        dmSys::Mkdir(dmTestUtil::MakeHostPath(testHome, sizeof(testHome), "testdir"), 0777);
        dmSys::Mkdir(dmTestUtil::MakeHostPath(testXDG, sizeof(testXDG), "testdir/xdg"), 0777);
        setenv("HOME", testHome, 1);
        setenv("XDG_DATA_HOME", testXDG, 1);
    };
    void TearDown()
    {
        char testPath[128];
        dmSys::RmTree(dmTestUtil::MakeHostPath(testPath, sizeof(testPath), "testdir"));
        if (originalHome)
            setenv("HOME", originalHome, 1);
        if (originalXDG)
            setenv("XDG_DATA_HOME", originalXDG, 1);
    };
};

//Check that the legacy support path works
TEST_F(dmSysXDG, LegacySupportPath)
{
    char path[128];
    char legacy[128];
    dmTestUtil::MakeHostPath(legacy, sizeof(legacy), "testdir/.Defold");
    dmSys::Mkdir(legacy, 0777);

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(legacy, path);
}

//Check that XDG support works
TEST_F(dmSysXDG, XDGSupportPath)
{
    char path[128];
    char xdg[128];
    dmTestUtil::MakeHostPath(xdg, sizeof(xdg), "testdir/xdg/Defold");

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(xdg, path);
}

//If application_folder at XDG exists, it has first priority
TEST_F(dmSysXDG, XDGBeforeAll)
{
    char path[128];
    char legacy[128];
    char xdg[128];
    char fallback[128];
    dmTestUtil::MakeHostPath(xdg, sizeof(xdg), "testdir/xdg/Defold");
    dmTestUtil::MakeHostPath(legacy, sizeof(legacy), "testdir/.Defold");
    dmTestUtil::MakeHostPath(fallback, sizeof(fallback), "testdir/.local/share/Defold");

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(xdg, path);
}

//If legacy application_folder exists, it has second priority
TEST_F(dmSysXDG, LegacyBeforeXDG)
{
    char path[128];
    char legacy[128];
    char fallback[128];
    dmTestUtil::MakeHostPath(legacy, sizeof(legacy), "testdir/.Defold");
    dmSys::Mkdir(legacy, 0777);
    dmTestUtil::MakeHostPath(fallback, sizeof(fallback), "testdir/.local/share/");

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(legacy, path);
}

// If neither legacy nor $XDG_DATA_HOME exist, fallback has priority
TEST_F(dmSysXDG, FallbackSupportPath)
{
    char path[128];
    char fallback[128];
    dmTestUtil::MakeHostPath(fallback, sizeof(fallback), "testdir/.local/share/Defold");
    dmSys::Mkdir(fallback, 0777);
    unsetenv("XDG_DATA_HOME");

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(fallback, path);
}

//If $XDG_DATA_HOME is set, it has priority over the fallback
TEST_F(dmSysXDG, XDGBeforeFallback)
{
    char path[128];
    char fallback[128];
    char xdg[128];
    dmTestUtil::MakeHostPath(xdg, sizeof(xdg), "testdir/xdg/Defold");
    dmTestUtil::MakeHostPath(fallback, sizeof(fallback), "testdir/.local/share/Defold");

    dmSys::Result result = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_STREQ(xdg, path);
}

#endif

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
