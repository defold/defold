// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>
#include <stdlib.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
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
    return jc_test_run_all();
}

