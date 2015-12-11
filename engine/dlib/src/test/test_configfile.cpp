#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/configfile.h"

#include <dlib/sol.h>

const char* DEFAULT_ARGV[] = { "test_engine" };

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


class ConfigTest : public ::testing::TestWithParam<TestParam>
{
public:

    dmConfigFile::HConfig config;
    dmConfigFile::Result r;
    char* m_Buffer;

    virtual void SetUp()
    {
        const uint32_t buffer_size = 1024 * 1024; // Big enough..
        m_Buffer = new char[buffer_size];
        TestParam param = GetParam();

        if (param.m_FromBuffer)
        {
            FILE* f = fopen(param.m_Url, "rb");
            ASSERT_NE((FILE*) 0, f);
            size_t file_size = fread(m_Buffer, 1, buffer_size, f);
            fclose(f);

            r = dmConfigFile::LoadFromBuffer(m_Buffer, file_size, param.m_Argc, param.m_Argv, &config);
        }
        else
        {
            r = dmConfigFile::Load(param.m_Url, param.m_Argc, param.m_Argv, &config);
        }


        if (r != dmConfigFile::RESULT_OK)
        {
            config = 0;
        }
    }

    virtual void TearDown()
    {
        delete [] m_Buffer;
        if (config)
        {
            dmConfigFile::Delete(config);
        }
    }
};

class Empty : public ConfigTest {};

TEST_P(Empty, Empty)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_NE((void*) 0, config);
}

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(Empty,
                        Empty,
                        ::testing::Values(TestParam("src/test/data/empty.config"),
                                          TestParam("src/test/data/empty.config", true),
                                          TestParam("http://localhost:7000/src/test/data/test.config")));
#else
INSTANTIATE_TEST_CASE_P(Empty,
                        Empty,
                        ::testing::Values(TestParam("src/test/data/empty.config"),
                                          TestParam("src/test/data/empty.config", true)));
#endif

class MissingFile : public ConfigTest {};

TEST_P(MissingFile, MissingFile)
{
    ASSERT_EQ(dmConfigFile::RESULT_FILE_NOT_FOUND, r);
}


#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(MissingFile,
                        MissingFile,
                        ::testing::Values(TestParam("does_not_exists"),
                                          TestParam("http://localhost:7000/does_not_exists")));
#else
INSTANTIATE_TEST_CASE_P(MissingFile,
                        MissingFile,
                        ::testing::Values(TestParam("does_not_exists")));
#endif

class NoSection : public ConfigTest {};

TEST_P(NoSection, NoSection)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_STREQ("123", dmConfigFile::GetString(config, ".foo", 0));
    ASSERT_STREQ("456", dmConfigFile::GetString(config, ".bar", 0));
}

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(NoSection,
                        NoSection,
                        ::testing::Values(TestParam("src/test/data/nosection.config"),
                                          TestParam("src/test/data/nosection.config", true),
                                          TestParam("http://localhost:7000/src/test/data/nosection.config")));
#else
INSTANTIATE_TEST_CASE_P(NoSection,
                        NoSection,
                        ::testing::Values(TestParam("src/test/data/nosection.config"),
                                          TestParam("src/test/data/nosection.config", true)));
#endif

class SectionError : public ConfigTest {};

TEST_P(SectionError, SectionError)
{
    ASSERT_NE(dmConfigFile::RESULT_OK, r);
}

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(SectionError,
                        SectionError,
                        ::testing::Values(TestParam("src/test/data/section_error.config"),
                                          TestParam("src/test/data/section_error.config", true),
                                          TestParam("http://localhost:7000/src/test/data/section_error.config")));
#else
INSTANTIATE_TEST_CASE_P(SectionError,
                        SectionError,
                        ::testing::Values(TestParam("src/test/data/section_error.config"),
                                          TestParam("src/test/data/section_error.config", true)));
#endif

class Test01 : public ConfigTest {};

TEST_P(Test01, Test01)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);

    ASSERT_STREQ("123", dmConfigFile::GetString(config, "main.foo", 0));
    ASSERT_EQ(123, dmConfigFile::GetInt(config, "main.foo", 0));

    ASSERT_STREQ("456", dmConfigFile::GetString(config, "sub.bar", 0));
    ASSERT_EQ(456, dmConfigFile::GetInt(config, "sub.bar", 0));
    ASSERT_STREQ("foo_bar", dmConfigFile::GetString(config, "sub.value", 0));
    ASSERT_STREQ("", dmConfigFile::GetString(config, "sub.bad_int1", 0));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int1", -1));
    ASSERT_EQ(-1, dmConfigFile::GetInt(config, "sub.bad_int2", -1));

    ASSERT_STREQ("missing_value", dmConfigFile::GetString(config, "missing_key", "missing_value"));
    ASSERT_EQ(1122, dmConfigFile::GetInt(config, "missing_int_key", 1122));
}

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(Test01,
                        Test01,
                        ::testing::Values(TestParam("src/test/data/test.config"),
                                          TestParam("src/test/data/test.config", true),
                                          TestParam("http://localhost:7000/src/test/data/test.config")));
#else
INSTANTIATE_TEST_CASE_P(Test01,
                        Test01,
                        ::testing::Values(TestParam("src/test/data/test.config"),
                                          TestParam("src/test/data/test.config", true)));
#endif

class MissingTrailingNewline : public ConfigTest {};

TEST_P(MissingTrailingNewline, MissingTrailingNewline)
{
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    ASSERT_STREQ("456", dmConfigFile::GetString(config, "main.foo", 0));
}

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(MissingTrailingNewline,
                        MissingTrailingNewline,
                        ::testing::Values(TestParam("src/test/data/missing_trailing_nl.config"),
                                          TestParam("src/test/data/missing_trailing_nl.config", true),
                                          TestParam("http://localhost:7000/src/test/data/missing_trailing_nl.config")));
#else
INSTANTIATE_TEST_CASE_P(MissingTrailingNewline,
                        MissingTrailingNewline,
                        ::testing::Values(TestParam("src/test/data/missing_trailing_nl.config"),
                                          TestParam("src/test/data/missing_trailing_nl.config", true)));
#endif

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

const char* COMMNAD_LINE_ARGV[] = { "an arg1", "--config=main.foo=1122", "an arg2", "--config=sub.newvalue=987", "an arg3", "--config=main.missing_value", "an arg4", "--config=main.empty_value=", "an arg5"  };
int COMMNAD_LINE_ARGC = sizeof(COMMNAD_LINE_ARGV) / sizeof(COMMNAD_LINE_ARGV[0]);

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(CommandLine,
                        CommandLine,
                        ::testing::Values(TestParam("src/test/data/test.config", COMMNAD_LINE_ARGC, COMMNAD_LINE_ARGV),
                                          TestParam("src/test/data/test.config", COMMNAD_LINE_ARGC, COMMNAD_LINE_ARGV, true),
                                          TestParam("http://localhost:7000/src/test/data/test.config", COMMNAD_LINE_ARGC, COMMNAD_LINE_ARGV)));
#else
INSTANTIATE_TEST_CASE_P(CommandLine,
                        CommandLine,
                        ::testing::Values(TestParam("src/test/data/test.config", COMMNAD_LINE_ARGC, COMMNAD_LINE_ARGV),
                                          TestParam("src/test/data/test.config", COMMNAD_LINE_ARGC, COMMNAD_LINE_ARGV, true)));
#endif

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    dmSol::Initialize();
    int r = RUN_ALL_TESTS();
    dmSol::FinalizeWithCheck();
    return r;
}

