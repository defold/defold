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

#define SUPPORT_RMTREE
#if defined(__EMSCRIPTEN__)
    #undef SUPPORT_RMTREE
#endif

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmSys::Result r) {
    return buffer + dmSnPrintf(buffer, buffer_len, "%s", dmSys::ResultToString(r));
}


// Unit test helpers
TEST(dmTestUtil, MakeHostPath)
{
    char path[128];

    dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists");
    ASSERT_STREQ(DM_HOSTFS "does_not_exists", path);
}
///////////////////////////////////////////////////////////

TEST(dmSys, Exists)
{
    char path[128];
    bool r;

    r = dmSys::Exists(dmTestUtil::MakeHostPath(path, sizeof(path), "src"));
    ASSERT_TRUE(r);

    r = dmSys::Exists(dmTestUtil::MakeHostPath(path, sizeof(path), "notexist"));
    ASSERT_FALSE(r);
}

TEST(dmSys, IsDir)
{
    char path[128];
    dmSys::Result r;

    // Check a directory
    dmTestUtil::MakeHostPath(path, sizeof(path), "src");
    ASSERT_TRUE(dmSys::Exists(path));
    r = dmSys::IsDir(path);
    ASSERT_EQ(dmSys::RESULT_OK, r);

    // Check a file
    dmTestUtil::MakeHostPath(path, sizeof(path), "wscript");
    ASSERT_TRUE(dmSys::Exists(path));
    r = dmSys::IsDir(path);
    ASSERT_EQ(dmSys::RESULT_UNKNOWN, r); // TODO: This api isn't very nice /MAWE
}

#if defined(SUPPORT_RMTREE)

TEST(dmSys, Mkdir)
{
    char path[128];
    dmSys::Result r;

    dmTestUtil::MakeHostPath(path, sizeof(path), "testdir");

    if (dmSys::Exists(path)) {
        r = dmSys::RmTree(path);
        ASSERT_EQ(dmSys::RESULT_OK, r);
    }

    r = dmSys::Mkdir(path, 0777);
    ASSERT_EQ(dmSys::RESULT_OK, r);
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::IsDir(path));

    r = dmSys::Mkdir(path, 0777);
    ASSERT_EQ(dmSys::RESULT_EXIST, r);

    dmTestUtil::MakeHostPath(path, sizeof(path), "not_exists");
    ASSERT_EQ(dmSys::RESULT_NOENT, dmSys::IsDir(path));

    r = dmSys::Mkdir(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/dir"), 0777);
    ASSERT_EQ(dmSys::RESULT_OK, r);

    r = dmSys::Mkdir(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/dir"), 0777);
    ASSERT_EQ(dmSys::RESULT_EXIST, r);

    r = dmSys::Rmdir(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/dir"));
    ASSERT_EQ(dmSys::RESULT_OK, r);
}
#endif

TEST(dmSys, Unlink)
{
    char path[128];

    dmSys::Result r;
    r = dmSys::Unlink(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/afile"));
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    ASSERT_NE(dmSys::RESULT_OK, dmSys::IsDir(path));

    FILE* f = fopen(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/afile"), "wb");
    ASSERT_NE((FILE*) 0, f);
    fclose(f);

    r = dmSys::Unlink(dmTestUtil::MakeHostPath(path, sizeof(path), "testdir/afile"));
    ASSERT_EQ(dmSys::RESULT_OK, r);
}

#if !defined(DM_TEST_NO_APPLICATION_SUPPORT_PATH) // Disabled until we can get the user paths

TEST(dmSys, GetApplicationSupportPathBuffer)
{
    char path[4];
    char discard[128];
    path[3] = '!';
    dmSys::Result result = dmSys::GetApplicationSupportPath("testing", path, 3);
    ASSERT_EQ(dmSys::RESULT_INVAL, result);
    ASSERT_EQ('\0', path[2]);
    ASSERT_EQ('!', path[3]);
}

TEST(dmSys, GetApplicationSavePathBuffer)
{
    char path[4];
    char discard[128];
    path[3] = '!';
    dmSys::Result result = dmSys::GetApplicationSavePath("testing", path, 3);
    ASSERT_EQ(dmSys::RESULT_INVAL, result);
    ASSERT_EQ('\0', path[2]);
    ASSERT_EQ('!', path[3]);
}

TEST(dmSys, GetApplicationSupportPath)
{
    char path[1024];
    dmSys::Result result = dmSys::GetApplicationSupportPath("testing", path, sizeof(path));
    ASSERT_EQ(dmSys::RESULT_OK, result);
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::IsDir(path));
}

#endif

int g_Argc;
char** g_Argv;

TEST(dmSys, GetResourcesPath)
{
    char path[DMPATH_MAX_PATH];
    dmSys::GetResourcesPath(g_Argc, g_Argv, path, sizeof(path));
    printf("GetResourcesPath: '%s'\n", path);
}

namespace dmSys
{
    void FillLanguageTerritory(const char* lang, struct SystemInfo* info);
}


TEST(dmSys, GetSystemInfo)
{
    dmSys::SystemInfo info;
    dmSys::GetSystemInfo(&info);
    dmSys::GetSecureInfo(&info);

    dmLogInfo("DeviceModel: '%s'", info.m_DeviceModel);
    dmLogInfo("SystemName: '%s'", info.m_SystemName);
    dmLogInfo("SystemVersion: '%s'", info.m_SystemVersion);
    dmLogInfo("SystemApiVersion: '%s'", info.m_ApiVersion);
    dmLogInfo("Language: '%s'", info.m_Language);
    dmLogInfo("Device Language: '%s'", info.m_DeviceLanguage);
    dmLogInfo("Territory: '%s'", info.m_Territory);
    dmLogInfo("GMT offset: '%d'", info.m_GmtOffset);
    dmLogInfo("Device identifier: '%s'", info.m_DeviceIdentifier);

    memset(&info, 0x0, sizeof(info));

#ifdef CHECK_LANG_TERR
#error "CHECK_LANG_TERR already defined"
#endif
#define CHECK_LANG_TERR(lang_str, lang, dev_lang, territory) \
    dmSys::FillLanguageTerritory(lang_str, &info); \
    ASSERT_STREQ(lang, info.m_Language); \
    ASSERT_STREQ(dev_lang, info.m_DeviceLanguage); \
    ASSERT_STREQ(territory, info.m_Territory);

    CHECK_LANG_TERR((const char*)0x0, "en", "en", "US");
    CHECK_LANG_TERR("", "en", "en", "US");
    CHECK_LANG_TERR("e", "e", "e", "");
    CHECK_LANG_TERR("sv", "sv", "sv", "");
    CHECK_LANG_TERR("sv_SE", "sv", "sv", "SE");
    CHECK_LANG_TERR("sv-SE", "sv", "sv", "SE");
    CHECK_LANG_TERR("zh_Hant_CN", "zh", "zh-Hant", "CN");
    CHECK_LANG_TERR("zh-Hant-CN", "zh", "zh-Hant", "CN");
    CHECK_LANG_TERR("zh_Hant-CN", "zh", "zh-Hant", "CN");
    CHECK_LANG_TERR("zh-Hant_CN", "zh", "zh-Hant", "CN");
    CHECK_LANG_TERR("zh_Hant-xxx_xxx_CN", "zh", "zh-Hant-xxx_xxx", "CN");

#undef CHECK_LANG_TERR

}

TEST(dmSys, EngineInfo)
{
    dmSys::EngineInfoParam engine_info;
    engine_info.m_Version = "1.2.1";
    engine_info.m_VersionSHA1 = "hellothere";
    engine_info.m_Platform = "testplatform";
    engine_info.m_IsDebug = true;
    dmSys::SetEngineInfo(engine_info);
    dmSys::EngineInfo info;
    dmSys::GetEngineInfo(&info);
    ASSERT_STREQ("1.2.1", info.m_Version);
    ASSERT_STREQ("hellothere", info.m_VersionSHA1);
    ASSERT_STREQ("testplatform", info.m_Platform);
    ASSERT_TRUE(info.m_IsDebug);
}

TEST(dmSys, LoadResource)
{
    char path[128];
    char buffer[1024 * 100];
    dmSys::Result r;
    uint32_t size;

    r = dmSys::LoadResource(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResource(dmTestUtil::MakeHostPath(path, sizeof(path), "."), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResource(dmTestUtil::MakeHostPath(path, sizeof(path), "wscript"), 0, 0, &size);
    ASSERT_EQ(dmSys::RESULT_INVAL, r);

    r = dmSys::LoadResource(dmTestUtil::MakeHostPath(path, sizeof(path), "wscript"), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_OK, r);
    uint32_t size2;
    r = dmSys::ResourceSize(dmTestUtil::MakeHostPath(path, sizeof(path), "wscript"), &size2);
    ASSERT_EQ(dmSys::RESULT_OK, r);
    ASSERT_EQ(size, size2);
    ASSERT_GT(size, 0);
}

TEST(dmSys, LoadResourcePartial)
{
    char path[128];
    uint8_t buffer[1024 * 100];
    dmSys::Result r;
    uint32_t nread;

    // Checking errors
    r = dmSys::LoadResourcePartial(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), 0, sizeof(buffer), buffer, &nread);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), &nread);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResourcePartial(dmTestUtil::MakeHostPath(path, sizeof(path), "."), 0, sizeof(buffer), buffer, &nread);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(dmTestUtil::MakeHostPath(path, sizeof(path), "does_not_exists"), &nread);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResourcePartial(dmTestUtil::MakeHostPath(path, sizeof(path), "wscript"), 0, 0, 0, &nread);
    ASSERT_EQ(dmSys::RESULT_INVAL, r);

    // Create a test file
    const char* datapath = dmTestUtil::MakeHostPath(path, sizeof(path), "testdata");

    uint8_t testdata[256];
    uint32_t testdatasize = sizeof(testdata);
    for (uint32_t i = 0; i < testdatasize; ++i)
        testdata[i] = (uint8_t)i;

    {
        FILE* f = fopen(datapath, "wb");
        ASSERT_NE((FILE*)0, f);
        fwrite(testdata, 1, testdatasize, f);
        fclose(f);
    }

    // Check reads of the file
    uint32_t chunk_size = 70; // 256 is not evenly divisible by 70
    uint32_t offset = 0;
    while (offset < testdatasize)
    {
        r = dmSys::LoadResourcePartial(datapath, offset, chunk_size, buffer, &nread);
        ASSERT_EQ(dmSys::RESULT_OK, r);

        ASSERT_ARRAY_EQ_LEN(&testdata[offset], buffer, nread);

        offset += nread;
    }
    ASSERT_EQ(testdatasize, offset);
}

int main(int argc, char **argv)
{
    g_Argc = argc;
    g_Argv = argv;

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
