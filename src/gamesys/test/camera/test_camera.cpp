#include "../test_gamesys.h"

class CameraTest : public GamesysTest
{
};

TEST_F(CameraTest, TestNewDelete)
{
    ASSERT_TRUE(true);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
