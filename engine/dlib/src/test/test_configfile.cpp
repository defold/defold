// Copyright 2020-2025 The Defold Foundation
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

#include <stdio.h>
#include <stdint.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/configfile.h>
#include <dlib/testutil.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#if (defined(GITHUB_CI) && defined(__MACH__)) || defined(_WIN32) || defined(DM_PLATFORM_VENDOR) || defined(__EMSCRIPTEN__)
#define DM_DISABLE_HTTPCLIENT_TESTS
#endif

const char* DEFAULT_ARGV[] = { "test_engine" };
int g_HttpPort = -1;

extern "C" void TestConfigfileExtension();

struct ExtensionInitializer
{
    ExtensionInitializer()
    {
        TestConfigfileExtension();
    }
} g_Initializer;

struct TestParam
{
    TestParam(const char* url, bool from_buffer = false)
    {
        m_Url = url;
        m_Argc = 1;
        m_Argv = DEFAULT_ARGV;
        m_FromBuffer = from_buffer;
    }

    TestParam(const char* url, int argc, const char* argv[], bool from_buffer = false)
    {
        m_Url = url;
        m_Argc = argc;
        m_Argv = argv;
        m_FromBuffer = false;
    }

    const char*  m_Url;
    int          m_Argc;
    const char** m_Argv;
    bool         m_FromBuffer;
};


class ConfigTest : public jc_test_params_class<TestParam>
{
public:

    dmConfigFile::HConfig config;
    dmConfigFile::Result r;
    char* m_Buffer;

    void SetUp() override
    {
        const uint32_t buffer_size = 1024 * 1024; // Big enough..
        m_Buffer = new char[buffer_size];
        const TestParam& param = GetParam();

        char path[1024];
        dmTestUtil::MakeHostPath(path, sizeof(path), param.m_Url);

        if (param.m_FromBuffer)
        {
            FILE* f = fopen(path, "rb");
            ASSERT_NE((FILE*) 0, f);
            size_t file_size = fread(m_Buffer, 1, buffer_size, f);
            fclose(f);

            r = dmConfigFile::LoadFromBuffer(m_Buffer, file_size, param.m_Argc, param.m_Argv, &config);
        }
        else
        {
            if (strstr(param.m_Url, "http") != 0) {
                dmSnPrintf(path, sizeof(path), param.m_Url, g_HttpPort);
            }

            r = dmConfigFile::Load(path, param.m_Argc, param.m_Argv, &config);
        }

        if (r != dmConfigFile::RESULT_OK)
        {
            config = 0;
        }
    }

    void TearDown() override
    {
        delete [] m_Buffer;
        m_Buffer = 0;
        if (config)
        {
            dmConfigFile::Delete(config);
            config = 0;
        }
    }
};

class Empty : public ConfigTest {};

TEST_P(Empty, Empty)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_NE((void*) 0, config);
}

const TestParam params_empty[] = {
    TestParam("src/test/data/empty.config"),
    TestParam("src/test/data/empty.config",false),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/test.config")
#endif
};

INSTANTIATE_TEST_CASE_P(Empty, Empty, jc_test_values_in(params_empty));

class MissingFile : public ConfigTest {};

TEST_P(MissingFile, MissingFile)
{
    ASSERT_EQ(dmConfigFile::RESULT_FILE_NOT_FOUND, r);
}

const TestParam params_missing_file[] = {
    TestParam("does_not_exist"),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/does_not_exist")
#endif
};

INSTANTIATE_TEST_CASE_P(MissingFile, MissingFile, jc_test_values_in(params_missing_file));

class NoSection : public ConfigTest {};

TEST_P(NoSection, NoSection)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_STREQ("123", dmConfigFile::GetString(config, ".foo", 0));
    ASSERT_STREQ("456", dmConfigFile::GetString(config, ".bar", 0));
}

const TestParam params_no_section[] = {
    TestParam("src/test/data/nosection.config"),
    TestParam("src/test/data/nosection.config", true),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/nosection.config")
#endif
};
INSTANTIATE_TEST_CASE_P(NoSection, NoSection, jc_test_values_in(params_no_section));

class SectionError : public ConfigTest {};

TEST_P(SectionError, SectionError)
{
    ASSERT_NE(dmConfigFile::RESULT_OK, r);
}

const TestParam params_section_error[] = {
    TestParam("src/test/data/section_error.config"),
    TestParam("src/test/data/section_error.config", true),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/section_error.config")
#endif
};

INSTANTIATE_TEST_CASE_P(SectionError, SectionError, jc_test_values_in(params_section_error));

class Test01 : public ConfigTest {};

TEST_P(Test01, Test01)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);

    ASSERT_STREQ("123", dmConfigFile::GetString(config, "main.foo", 0));
    ASSERT_EQ(123, dmConfigFile::GetInt(config, "main.foo", 0));

    ASSERT_STREQ("#value", dmConfigFile::GetString(config, "comments.pound", 0));
    ASSERT_STREQ(";value", dmConfigFile::GetString(config, "comments.semi", 0));

    ASSERT_STREQ("456", dmConfigFile::GetString(config, "sub.bar", 0));
    ASSERT_EQ(456, dmConfigFile::GetInt(config, "sub.bar", 0));
    ASSERT_STREQ("foo_bar", dmConfigFile::GetString(config, "sub.value", 0));
    ASSERT_STREQ("", dmConfigFile::GetString(config, "sub.bad_int1", 0));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int1", -1));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int2", -1));

    ASSERT_STREQ("missing_value", dmConfigFile::GetString(config, "missing_key", "missing_value"));
    ASSERT_EQ(1122, dmConfigFile::GetInt(config, "missing_int_key", 1122));

    // The extension plugin hooks are disabled
    ASSERT_STREQ("hello", dmConfigFile::GetString(config, "ext.string", 0));
    ASSERT_STREQ("42", dmConfigFile::GetString(config, "ext.int", 0));
    ASSERT_STREQ("1.0", dmConfigFile::GetString(config, "ext.float", 0));

    ASSERT_EQ(42, dmConfigFile::GetInt(config, "ext.int", 0));
    ASSERT_EQ(1.0f, dmConfigFile::GetFloat(config, "ext.float", 0));
    ASSERT_EQ(0, dmConfigFile::GetInt(config, "ext.virtual", 0));
}

TEST_P(Test01, TestApiC)
{
    ASSERT_STREQ("123", ConfigFileGetString(config, "main.foo", 0));
    ASSERT_EQ(123, ConfigFileGetInt(config, "main.foo", 0));

    ASSERT_STREQ("#value", ConfigFileGetString(config, "comments.pound", 0));
    ASSERT_STREQ(";value", ConfigFileGetString(config, "comments.semi", 0));

    ASSERT_STREQ("456", ConfigFileGetString(config, "sub.bar", 0));
    ASSERT_EQ(456, ConfigFileGetInt(config, "sub.bar", 0));
    ASSERT_STREQ("foo_bar", ConfigFileGetString(config, "sub.value", 0));
    ASSERT_STREQ("", ConfigFileGetString(config, "sub.bad_int1", 0));
    ASSERT_EQ(-1, ConfigFileGetInt(config, "sub.bad_int1", -1));
    ASSERT_EQ(-1, ConfigFileGetInt(config, "sub.bad_int2", -1));

    ASSERT_STREQ("missing_value", ConfigFileGetString(config, "missing_key", "missing_value"));
    ASSERT_EQ(1122, ConfigFileGetInt(config, "missing_int_key", 1122));

    // The extension plugin hooks are disabled
    ASSERT_STREQ("hello", ConfigFileGetString(config, "ext.string", 0));
    ASSERT_STREQ("42", ConfigFileGetString(config, "ext.int", 0));
    ASSERT_STREQ("1.0", ConfigFileGetString(config, "ext.float", 0));

    ASSERT_EQ(42, ConfigFileGetInt(config, "ext.int", 0));
    ASSERT_EQ(1.0f, ConfigFileGetFloat(config, "ext.float", 0));
    ASSERT_EQ(0, ConfigFileGetInt(config, "ext.virtual", 0));
}

const TestParam params_test01[] = {
    TestParam("src/test/data/test.config"),
    TestParam("src/test/data/test.config", true),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/test.config")
#endif
};

INSTANTIATE_TEST_CASE_P(Test01, Test01, jc_test_values_in(params_test01));


class MissingTrailingNewline : public ConfigTest {};

TEST_P(MissingTrailingNewline, MissingTrailingNewline)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_STREQ("456", dmConfigFile::GetString(config, "main.foo", 0));
}

const TestParam params_missing_trailing_nl[] = {
    TestParam("src/test/data/missing_trailing_nl.config"),
    TestParam("src/test/data/missing_trailing_nl.config", true),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/missing_trailing_nl.config")
#endif
};

INSTANTIATE_TEST_CASE_P(MissingTrailingNewline, MissingTrailingNewline, jc_test_values_in(params_missing_trailing_nl));


class CommandLine : public ConfigTest {};

TEST_P(CommandLine, CommandLine)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);

    ASSERT_STREQ("", dmConfigFile::GetString(config, "main.empty_value", "XYZ"));
    ASSERT_STREQ("XYZ", dmConfigFile::GetString(config, "main.missing_value", "XYZ"));

    ASSERT_STREQ("1122", dmConfigFile::GetString(config, "main.foo", 0));
    ASSERT_EQ(1122, dmConfigFile::GetInt(config, "main.foo", 0));

    ASSERT_STREQ("456", dmConfigFile::GetString(config, "sub.bar", 0));
    ASSERT_EQ(456, dmConfigFile::GetInt(config, "sub.bar", 0));
    ASSERT_STREQ("foo_bar", dmConfigFile::GetString(config, "sub.value", 0));
    ASSERT_STREQ("", dmConfigFile::GetString(config, "sub.bad_int1", 0));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int1", -1));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int2", -1));
    ASSERT_EQ(987, dmConfigFile::GetInt(config, "sub.newvalue", 0));

    ASSERT_STREQ("missing_value", dmConfigFile::GetString(config, "missing_key", "missing_value"));
    ASSERT_EQ(1122, dmConfigFile::GetInt(config, "missing_int_key", 1122));
}

const char* COMMAND_LINE_ARGV[] = { "an arg1", "--config=main.foo=1122", "an arg2", "--config=sub.newvalue=987", "an arg3", "--config=main.missing_value", "an arg4", "--config=main.empty_value=", "an arg5"  };
int COMMAND_LINE_ARGC = sizeof(COMMAND_LINE_ARGV) / sizeof(COMMAND_LINE_ARGV[0]);

const TestParam params_command_line[] = {
    TestParam("src/test/data/test.config", COMMAND_LINE_ARGC, COMMAND_LINE_ARGV),
    TestParam("src/test/data/test.config", COMMAND_LINE_ARGC, COMMAND_LINE_ARGV, true),
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    TestParam("http://localhost:%d/src/test/data/test.config", COMMAND_LINE_ARGC, COMMAND_LINE_ARGV)
#endif
};

INSTANTIATE_TEST_CASE_P(CommandLine, CommandLine, jc_test_values_in(params_command_line));


class LongValue : public ConfigTest {};

TEST_P(LongValue, LongValue)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    int expected_length = (50 * 200) + 49;
    const char* long_value = dmConfigFile::GetString(config, "section.long", 0);
    int length = strlen(long_value);
    ASSERT_EQ(expected_length, length);
}

const TestParam params_long_value[] = {
    TestParam("src/test/data/long_value.config"),
};

INSTANTIATE_TEST_CASE_P(LongValue, LongValue, jc_test_values_in(params_long_value));

static int g_ExtensionTestEnabled = 0;
static int g_ExtensionTestCreated = 0;
static int g_ExtensionTestDestroyed = 0;
static const char* g_ExtensionTestString = "world";

static void TestCreate(dmConfigFile::HConfig config)
{
    if (!g_ExtensionTestEnabled) return;
    g_ExtensionTestCreated++;
}
static void TestDestroy(dmConfigFile::HConfig config)
{
    if (!g_ExtensionTestEnabled) return;
    g_ExtensionTestDestroyed++;
}
static bool TestGetString(dmConfigFile::HConfig config, const char* key, const char* default_value, const char** out)
{
    if (!g_ExtensionTestEnabled) return false;
    if (strstr(key, "ext.") != key)
        return false;

    if (strcmp("ext.string", key) == 0)
    {
        *out = g_ExtensionTestString;
        return true;
    }
    return false;
}
static bool TestGetInt(dmConfigFile::HConfig config, const char* key, int32_t default_value, int32_t* out)
{
    if (!g_ExtensionTestEnabled) return false;
    if (strstr(key, "ext.") != key)
        return false;
    if (strcmp("ext.virtual", key) == 0)
    {
        *out = 1301;
        return true;
    }
    *out = default_value * 2;
    return true;
}
static bool TestGetFloat(dmConfigFile::HConfig config, const char* key, float default_value, float* out)
{
    if (!g_ExtensionTestEnabled) return false;
    if (strstr(key, "ext.") != key)
        return false;
    if (strcmp("ext.virtual", key) == 0)
    {
        *out = 4096;
        return true;
    }
    *out = default_value * 2;
    return true;
}

DM_DECLARE_CONFIGFILE_EXTENSION(TestConfigfileExtension, "TestConfigfileExtension", TestCreate, TestDestroy, TestGetString, TestGetInt, TestGetFloat);

class ConfigfileExtension : public ConfigTest {
    void SetUp() override
    {
        g_ExtensionTestEnabled = 1;
        ConfigTest::SetUp();

        ASSERT_EQ(g_ExtensionTestCreated, 1);
    }
    void TearDown() override
    {
        ConfigTest::TearDown();
        g_ExtensionTestEnabled = 0;

        ASSERT_EQ(g_ExtensionTestDestroyed, 1);
    }
};

TEST_P(ConfigfileExtension, ConfigfileExtension)
{
    ASSERT_STREQ("world", dmConfigFile::GetString(config, "ext.string", 0));
    ASSERT_STREQ("42", dmConfigFile::GetString(config, "ext.int", 0));
    ASSERT_STREQ("1.0", dmConfigFile::GetString(config, "ext.float", 0));

    ASSERT_EQ(84, dmConfigFile::GetInt(config, "ext.int", 0));
    ASSERT_EQ(2.0f, dmConfigFile::GetFloat(config, "ext.float", 0));

    ASSERT_EQ(1301, dmConfigFile::GetInt(config, "ext.virtual", 0));
    ASSERT_EQ(4096.0f, dmConfigFile::GetFloat(config, "ext.virtual", 0));
}

const TestParam params_extension[] = {
    TestParam("src/test/data/test.config"),
};

INSTANTIATE_TEST_CASE_P(ConfigfileExtension, ConfigfileExtension, jc_test_values_in(params_extension));



static void Usage()
{
    dmLogError("Usage: <exe> <config>");
    dmLogError("Be sure to start the http server before starting this test.");
    dmLogError("You can use the config file created by the server");
}

int main(int argc, char **argv)
{
#if !defined(DM_DISABLE_HTTPCLIENT_TESTS)
    if(argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", path);
            Usage();
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, 0, 0);
        dmConfigFile::Delete(config);
    }
    else
    {
        Usage();
        return 1;
    }
#endif

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
