#include <stdio.h>
#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/hash.h>

extern "C" int csRunTestsHash();
extern "C" uint64_t csHashString64(const char* s);


TEST(CSharp, Hash)
{
    ASSERT_EQ(0, csRunTestsHash());

    ASSERT_EQ(dmHashString64("Hello World!"), csHashString64("Hello World!"));
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
