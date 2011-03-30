#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/sys.h"

TEST(dmSys, Mkdir)
{
    dmSys::Result r;
    r = dmSys::Mkdir("tmp/dir", 0777);
    ASSERT_EQ(dmSys::RESULT_OK, r);

    r = dmSys::Mkdir("tmp/dir", 0777);
    ASSERT_EQ(dmSys::RESULT_EXIST, r);

    r = dmSys::Rmdir("tmp/dir");
    ASSERT_EQ(dmSys::RESULT_OK, r);
}

TEST(dmSys, Unlink)
{
    dmSys::Result r;
    r = dmSys::Unlink("tmp/afile");
    ASSERT_EQ(dmSys::RESULT_NOENT, r);

    FILE* f = fopen("tmp/afile", "wb");
    ASSERT_NE((FILE*) 0, f);
    fclose(f);

    r = dmSys::Unlink("tmp/afile");
    ASSERT_EQ(dmSys::RESULT_OK, r);
}

int main(int argc, char **argv)
{
    system("python src/test/test_sys.py");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

