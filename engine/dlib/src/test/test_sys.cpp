// Copyright 2020-2022 The Defold Foundation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/dstrings.h>
#include <dlib/sys.h>
#include <dlib/sys_internal.h>
#include <dlib/path.h>
#include <dlib/log.h>

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmSys::Result r) {
    return buffer + dmSnPrintf(buffer, buffer_len, "%s", dmSys::ResultToString(r));
}

static const char* MakeSupportPath(char* dst, uint32_t dst_len, const char* path)
{
#if defined(__NX__)
    dmSys::GetApplicationSupportPath("test_sys", dst, dst_len);
    dmStrlCat(dst, path, dst_len);
#else
    // storing unit test data elsewhere seems unnecessary
    dmStrlCpy(dst, path, dst_len);
#endif
    return dst;
}

static const char* MakeHostPath(char* dst, uint32_t dst_len, const char* path)
{
#if defined(__NX__)
    dmStrlCpy(dst, "host:/", dst_len);
    dmStrlCat(dst, path, dst_len);
    return dst;
#else
    return path;
#endif
}

TEST(dmSys, Mkdir)
{
    char path[128];

    dmSys::Result r;
    r = dmSys::Mkdir(MakeSupportPath(path, sizeof(path), "tmp"), 0777);
#if !defined(DM_NO_SYSTEM_FUNCTION)
    ASSERT_EQ(dmSys::RESULT_EXIST, r);
#else
    ASSERT_EQ(dmSys::RESULT_OK, r);
#endif
    r = dmSys::Mkdir(MakeSupportPath(path, sizeof(path), "tmp/dir"), 0777);
    ASSERT_EQ(dmSys::RESULT_OK, r);

    r = dmSys::Mkdir(MakeSupportPath(path, sizeof(path), "tmp/dir"), 0777);
    ASSERT_EQ(dmSys::RESULT_EXIST, r);

    r = dmSys::Rmdir(MakeSupportPath(path, sizeof(path), "tmp/dir"));
    ASSERT_EQ(dmSys::RESULT_OK, r);
}

TEST(dmSys, Unlink)
{
    char path[128];

    dmSys::Result r;
    r = dmSys::Unlink(MakeSupportPath(path, sizeof(path), "tmp/afile"));
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    FILE* f = fopen(MakeSupportPath(path, sizeof(path), "tmp/afile"), "wb");
    ASSERT_NE((FILE*) 0, f);
    fclose(f);

    r = dmSys::Unlink(MakeSupportPath(path, sizeof(path), "tmp/afile"));
    ASSERT_EQ(dmSys::RESULT_OK, r);
}

TEST(dmSys, GetApplicationSupportPathBuffer)
{
    char path[4];
    path[3] = '!';
    dmSys::Result result = dmSys::GetApplicationSupportPath("testing", path, 3);
    ASSERT_EQ(dmSys::RESULT_INVAL, result);
    ASSERT_EQ('\0', path[2]);
    ASSERT_EQ('!', path[3]);
}

TEST(dmSys, GetApplicationSavePathBuffer)
{
    char path[4];
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
    struct stat stat_data;
    int ret = stat(path, &stat_data);
    ASSERT_EQ(0, ret);
    ASSERT_EQ((uint32_t)S_IFDIR, stat_data.st_mode & S_IFDIR);
}

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

    r = dmSys::LoadResource(MakeHostPath(path, sizeof(path), "does_not_exists"), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(MakeHostPath(path, sizeof(path), "does_not_exists"), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResource(MakeHostPath(path, sizeof(path), "."), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);
    r = dmSys::ResourceSize(MakeHostPath(path, sizeof(path), "does_not_exists"), &size);
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    r = dmSys::LoadResource(MakeHostPath(path, sizeof(path), "wscript"), 0, 0, &size);
    ASSERT_EQ(dmSys::RESULT_INVAL, r);

    r = dmSys::LoadResource(MakeHostPath(path, sizeof(path), "wscript"), buffer, sizeof(buffer), &size);
    ASSERT_EQ(dmSys::RESULT_OK, r);
    uint32_t size2;
    r = dmSys::ResourceSize(MakeHostPath(path, sizeof(path), "wscript"), &size2);
    ASSERT_EQ(dmSys::RESULT_OK, r);
    ASSERT_EQ(size, size2);
    ASSERT_GT(size, 0);
}

int main(int argc, char **argv)
{
    g_Argc = argc;
    g_Argv = argv;
#if !defined(DM_NO_SYSTEM_FUNCTION)
    system("python src/test/test_sys.py"); // creates the ./tmp folder
#endif
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
