#include <gtest/gtest.h>

#include "../script.h"

TEST(gamesys, Script)
{

    using namespace Script;

    HScript s;

    s = New(NULL);
    ASSERT_EQ((int)s, 0);

}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    Script::Initialize();
    int ret = RUN_ALL_TESTS();
    Script::Finalize();
}
