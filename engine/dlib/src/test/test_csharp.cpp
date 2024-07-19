#include <stdio.h>
#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/configfile.h>
#include <dlib/configfile.h>

extern "C" int      csRunTestsHash();
extern "C" uint64_t csHashString64(const char* s);

extern "C" int      csRunTestsConfigFile(HConfigFile config);

TEST(CSharp, Hash)
{
    ASSERT_EQ(0, csRunTestsHash());
    ASSERT_EQ(dmHashString64("Hello World!"), csHashString64("Hello World!"));
}

static const char* CONFIG_BUFFER=
"[test]\n"
"my_int = 1\n"
"my_float = 2\n"
"my_string = hello\n"
"";

TEST(CSharp, ConfigFile)
{
    HConfigFile config;

    dmConfigFile::Result result = dmConfigFile::LoadFromBuffer(CONFIG_BUFFER, strlen(CONFIG_BUFFER), 0, 0, &config);
    ASSERT_EQ(dmConfigFile::RESULT_OK, result);

    // C
    ASSERT_EQ(1, dmConfigFile::GetInt(config, "test.my_int", -1));
    ASSERT_EQ(2.0f, dmConfigFile::GetFloat(config, "test.my_float", -1));
    ASSERT_STREQ("hello", dmConfigFile::GetString(config, "test.my_string", "wrong"));

    // C#
    ASSERT_EQ(0, csRunTestsConfigFile(config));

    dmConfigFile::Delete(config);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
