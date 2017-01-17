#include <stdint.h>
#include <gtest/gtest.h>
#include "../liveupdate.h"
#include "../liveupdate_private.h"

TEST(dmLiveUpdate, Test)
{
    ASSERT_EQ(1, 1);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
