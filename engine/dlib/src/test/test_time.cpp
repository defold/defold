#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/time.h"

TEST(dmTime, Sleep)
{
    dmTime::Sleep(1);
}

TEST(dmTime, GetTime)
{
    uint64_t start = dmTime::GetTime();
    dmTime::Sleep(200000);
    uint64_t end = dmTime::GetTime();
    ASSERT_NEAR((double) 200000, (double) (end-start), (double) 40000);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return JC_TEST_RUN_ALL();
}

