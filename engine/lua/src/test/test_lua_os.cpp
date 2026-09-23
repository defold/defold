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

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

class LuaOSTest : public jc_test_base_class
{
public:
    void SetUp()
    {
        const char* tmpdir = getenv("TMPDIR");
        m_OriginalTmpdir = tmpdir ? strdup(tmpdir) : 0;
        if (!tmpdir || !*tmpdir)
        {
            ASSERT_GT(confstr(_CS_DARWIN_USER_TEMP_DIR, m_Directory, sizeof(m_Directory)), 0U);
            tmpdir = m_Directory;
        }
        char directory[PATH_MAX];
        snprintf(directory, sizeof(directory), "%s/lua_os_test_XXXXXX", tmpdir);
        ASSERT_NE((char*)0, mkdtemp(directory));
        strcpy(m_Directory, directory);
        ASSERT_EQ(0, setenv("TMPDIR", m_Directory, 1));
        m_Lua = luaL_newstate();
        luaL_openlibs(m_Lua);
    }

    void TearDown()
    {
        lua_close(m_Lua);
        if (m_OriginalTmpdir)
            setenv("TMPDIR", m_OriginalTmpdir, 1);
        else
            unsetenv("TMPDIR");
        free(m_OriginalTmpdir);
        ASSERT_EQ(0, rmdir(m_Directory));
    }

    char* m_OriginalTmpdir;
    char m_Directory[PATH_MAX];
    lua_State* m_Lua;
};

TEST_F(LuaOSTest, TemporaryFiles)
{
    // Replacing tmpnam with mkstemp must reserve distinct, writable files with
    // owner-only permissions in TMPDIR. Accept TMPDIR with or without a trailing
    // slash so file creation does not depend on how the environment is set up.
    for (int trailing_slash = 0; trailing_slash < 2; ++trailing_slash)
    {
        char directory[PATH_MAX];
        snprintf(directory, sizeof(directory), "%s%s", m_Directory, trailing_slash ? "/" : "");
        ASSERT_EQ(0, setenv("TMPDIR", directory, 1));
        ASSERT_EQ(0, luaL_dostring(m_Lua, "return os.tmpname(), os.tmpname()"));
        const char* first = lua_tostring(m_Lua, -2);
        const char* second = lua_tostring(m_Lua, -1);
        ASSERT_NE(0, strcmp(first, second));
        ASSERT_EQ(0, strncmp(first, m_Directory, strlen(m_Directory)));
        ASSERT_EQ('/', first[strlen(m_Directory)]);
        struct stat info;
        ASSERT_EQ(0, stat(first, &info));
        ASSERT_EQ(0600, info.st_mode & 0777);
        FILE* file = fopen(first, "w");
        ASSERT_NE((FILE*)0, file);
        ASSERT_EQ(4U, fwrite("test", 1, 4, file));
        ASSERT_EQ(0, fclose(file));
        ASSERT_EQ(0, remove(first));
        ASSERT_EQ(0, remove(second));
        lua_settop(m_Lua, 0);
    }
}

TEST_F(LuaOSTest, TemporaryFileDescriptorsAreClosed)
{
    // mkstemp opens a descriptor, but os.tmpname only returns the path. Compare
    // the next available descriptor before and after to catch a leak on each call.
    int before = open(m_Directory, O_RDONLY);
    ASSERT_GE(before, 0);
    ASSERT_EQ(0, close(before));
    ASSERT_EQ(0, luaL_dostring(m_Lua, "return os.tmpname()"));
    int after = open(m_Directory, O_RDONLY);
    ASSERT_GE(after, 0);
    ASSERT_EQ(0, close(after));
    ASSERT_EQ(before, after);
    ASSERT_EQ(0, remove(lua_tostring(m_Lua, -1)));
}

TEST_F(LuaOSTest, InvalidTemporaryDirectory)
{
    // An unusable TMPDIR must raise a Lua error instead of returning a filename
    // that was never created, so scripts can handle the failure.
    char missing[PATH_MAX];
    snprintf(missing, sizeof(missing), "%s/missing", m_Directory);
    ASSERT_EQ(0, setenv("TMPDIR", missing, 1));
    ASSERT_NE(0, luaL_dostring(m_Lua, "return os.tmpname()"));
}

TEST_F(LuaOSTest, TemporaryDirectoryTooLong)
{
    // A TMPDIR that leaves no room for the filename must raise a Lua error.
    // This guards the fixed-size path buffer against truncation or overflow.
    char directory[PATH_MAX + 1];
    memset(directory, 'x', sizeof(directory) - 1);
    directory[sizeof(directory) - 1] = 0;
    ASSERT_EQ(0, setenv("TMPDIR", directory, 1));
    ASSERT_NE(0, luaL_dostring(m_Lua, "return os.tmpname()"));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
