#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc/test.h>
#include "../dlib/path.h"

TEST(dmPathNormalize, Basic)
{
    char path[DMPATH_MAX_PATH];

    dmPath::Normalize("", path, sizeof(path));
    ASSERT_STREQ("", path);

    // forward slash

    dmPath::Normalize("/foo", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("//foo", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("foo", path, sizeof(path));
    ASSERT_STREQ("foo", path);

    dmPath::Normalize("/foo//", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("/foo//a", path, sizeof(path));
    ASSERT_STREQ("/foo/a", path);

    dmPath::Normalize("///", path, sizeof(path));
    ASSERT_STREQ("/", path);

    // backslash

    dmPath::Normalize("\\foo", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("\\\\foo", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("foo", path, sizeof(path));
    ASSERT_STREQ("foo", path);

    dmPath::Normalize("\\foo\\\\", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Normalize("\\foo\\\\a", path, sizeof(path));
    ASSERT_STREQ("/foo/a", path);

    dmPath::Normalize("\\\\\\", path, sizeof(path));
    ASSERT_STREQ("/", path);

    dmPath::Normalize("c:\\foo\\\\a", path, sizeof(path));
    ASSERT_STREQ("c:/foo/a", path);
}

TEST(dmPathNormalize, Buffer)
{
    char path[DMPATH_MAX_PATH];

    path[2] = '1';
    dmPath::Normalize("/x", path, 2);
    ASSERT_STREQ("/", path);
    ASSERT_EQ('1', path[2]);

    path[1] = '2';
    dmPath::Normalize("", path, 2);
    ASSERT_STREQ("", path);
    ASSERT_EQ('2', path[1]);

    path[4] = '3';
    dmPath::Normalize("/xy///", path, 4);
    ASSERT_STREQ("/xy", path);
    ASSERT_EQ('3', path[4]);
}

TEST(dmPathDirname, Basic)
{
    char path[DMPATH_MAX_PATH];

    dmPath::Dirname("", path, sizeof(path));
    ASSERT_STREQ("", path);

    dmPath::Dirname("foo", path, sizeof(path));
    ASSERT_STREQ("", path);

    dmPath::Dirname("/", path, sizeof(path));
    ASSERT_STREQ("/", path);

    dmPath::Dirname(".", path, sizeof(path));
    ASSERT_STREQ(".", path);

    dmPath::Dirname("./", path, sizeof(path));
    ASSERT_STREQ(".", path);

    dmPath::Dirname("/foo/bar", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Dirname("//foo//bar///", path, sizeof(path));
    ASSERT_STREQ("/foo", path);

    dmPath::Dirname("c:\\\\foo\\\\bar\\", path, sizeof(path));
    ASSERT_STREQ("c:/foo", path);
}

TEST(dmPathConcat, Basic)
{
    char path[DMPATH_MAX_PATH];
    dmPath::Concat("foo", "bar", path, sizeof(path));
    ASSERT_STREQ("foo/bar", path);

    dmPath::Concat("/foo//", "/bar///", path, sizeof(path));
    ASSERT_STREQ("/foo/bar", path);

    dmPath::Concat("", "", path, sizeof(path));
    ASSERT_STREQ("", path);

    dmPath::Concat("", "a", path, sizeof(path));
    ASSERT_STREQ("a", path);

    dmPath::Concat("a", "", path, sizeof(path));
    ASSERT_STREQ("a", path);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return JC_TEST_RUN_ALL();
}

