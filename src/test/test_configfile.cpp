#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/configfile.h"

TEST(ConfigFile, Empty)
{
    dmConfigFile::HConfig config;
    dmConfigFile::Result r = dmConfigFile::Load("src/test/data/empty.config", 0, (const char**) 0, &config);
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);
    dmConfigFile::Delete(config);
}

TEST(ConfigFile, NoSection)
{
    dmConfigFile::HConfig config;
    dmConfigFile::Result r = dmConfigFile::Load("src/test/data/nosection.config", 0, (const char**) 0, &config);
    ASSERT_EQ(dmConfigFile::RESULT_OK, r);

    ASSERT_STREQ("123", dmConfigFile::GetString(config, ".foo", 0));
    ASSERT_STREQ("456", dmConfigFile::GetString(config, ".bar", 0));
    dmConfigFile::Delete(config);
}

TEST(ConfigFile, SectionError)
{
    dmConfigFile::HConfig config;
    dmConfigFile::Result r = dmConfigFile::Load("src/test/data/section_error.config", 0, (const char**) 0, &config);
    ASSERT_NE(dmConfigFile::RESULT_OK, r);
}

TEST(ConfigFile, Test)
{
    dmConfigFile::HConfig config;
    dmConfigFile::Result r = dmConfigFile::Load("src/test/data/test.config", 0, (const char**) 0, &config);
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
    dmConfigFile::Delete(config);
}

TEST(ConfigFile, CommandLine)
{
    int argc = 4;
    const char* argv[] = { "--config=main.foo=1122", "--config=sub.newvalue=987", "--config=main.missing_value", "--config=main.empty_value=" };

    dmConfigFile::HConfig config;
    dmConfigFile::Result r = dmConfigFile::Load("src/test/data/test.config", argc, argv, &config);
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
    dmConfigFile::Delete(config);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

