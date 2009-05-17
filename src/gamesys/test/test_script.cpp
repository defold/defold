#include <gtest/gtest.h>

#include "../script.h"

char script1[] =
{
        "def Update():\n"
        "   print \"script 1\""
};

char script2[] =
{
        "def Update():\n"
        "   print \"script 2\""
};

TEST(gamesys, Script)
{
    Script::HScript s1;
    s1 = Script::New(NULL);
    ASSERT_EQ((int)s1, 0);

    Script::HScript s2;
    s2 = Script::New(script1);
    ASSERT_NE((int)s2, 0);

    Script::HScript s3;
    s3 = Script::New(script2);
    ASSERT_NE((int)s3, 0);


    PyObject* args = PyTuple_New(0);
    PyObject* pArgs;

    bool r1, r2;
    r1 = Script::Run(s2, "Update", pArgs, args);
    ASSERT_EQ((int)r1, 1);

    r2 = Script::Run(s3, "Update", pArgs, args);
    ASSERT_EQ((int)r2, 1);

    r1 = Script::Run(s2, "Update", pArgs, args);
    ASSERT_EQ((int)r1, 1);

    r2 = Script::Run(s3, "Update", pArgs, args);
    ASSERT_EQ((int)r2, 1);

    r2 = Script::Run(s3, "Update", pArgs, args);
    ASSERT_EQ((int)r2, 1);

    r1 = Script::Run(s2, "Update", pArgs, args);
    ASSERT_EQ((int)r1, 1);

}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    Script::Initialize();
    int ret = RUN_ALL_TESTS();
    Script::Finalize();
}
