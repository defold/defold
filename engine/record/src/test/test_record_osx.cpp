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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <record/record_private.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

class RecordOutputTest : public jc_test_base_class
{
protected:
    char m_Directory[1024];
    char m_Path[1024];
    char m_Child[1024];

    void SetUp()
    {
        const char* tmpdir = getenv("TMPDIR");
        if (!tmpdir || !tmpdir[0])
            tmpdir = "/tmp";
        ASSERT_LT(snprintf(m_Directory, sizeof(m_Directory), "%s/record-output-XXXXXX", tmpdir), (int)sizeof(m_Directory));
        ASSERT_NE((char*)0, mkdtemp(m_Directory));
        ASSERT_LT(snprintf(m_Path, sizeof(m_Path), "%s/capture-\xc3\xa9.mp4", m_Directory), (int)sizeof(m_Path));
        ASSERT_LT(snprintf(m_Child, sizeof(m_Child), "%s/saved-game.txt", m_Path), (int)sizeof(m_Child));
    }

    void TearDown()
    {
        unlink(m_Child);
        rmdir(m_Path);
        unlink(m_Path);
        ASSERT_EQ(0, rmdir(m_Directory));
    }
};

// A new Unicode output path remains usable without creating a premature MP4.
TEST_F(RecordOutputTest, NewOutput)
{
    dmRecord::Status status = {};
    ASSERT_TRUE(dmRecord::PrepareOutputFile(m_Path, &status));
    ASSERT_EQ(0, status.m_Failure[0]);
    ASSERT_NE(0, access(m_Path, F_OK));
}

// Recording may replace an existing regular file without removing its parent directory.
TEST_F(RecordOutputTest, ReplaceFile)
{
    FILE* file = fopen(m_Path, "wb");
    ASSERT_NE((FILE*)0, file);
    ASSERT_EQ(0, fclose(file));
    dmRecord::Status status = {};
    ASSERT_TRUE(dmRecord::PrepareOutputFile(m_Path, &status));
    ASSERT_NE(0, access(m_Path, F_OK));
    ASSERT_EQ(0, access(m_Directory, F_OK));
}

// Directory targets must preserve their contents instead of being recursively deleted.
TEST_F(RecordOutputTest, PreserveDirectoryContents)
{
    ASSERT_EQ(0, mkdir(m_Path, 0700));
    FILE* file = fopen(m_Child, "wb");
    ASSERT_NE((FILE*)0, file);
    ASSERT_NE(EOF, fputs("saved game", file));
    ASSERT_EQ(0, fclose(file));
    dmRecord::Status status = {};
    ASSERT_FALSE(dmRecord::PrepareOutputFile(m_Path, &status));
    ASSERT_NE(0, status.m_Failure[0]);
    file = fopen(m_Child, "rb");
    ASSERT_NE((FILE*)0, file);
    char contents[16];
    ASSERT_NE((char*)0, fgets(contents, sizeof(contents), file));
    ASSERT_EQ(0, fclose(file));
    ASSERT_STREQ("saved game", contents);
}

// Empty directory targets must also be rejected and left in place.
TEST_F(RecordOutputTest, PreserveEmptyDirectory)
{
    ASSERT_EQ(0, mkdir(m_Path, 0700));
    dmRecord::Status status = {};
    ASSERT_FALSE(dmRecord::PrepareOutputFile(m_Path, &status));
    ASSERT_NE(0, status.m_Failure[0]);
    struct stat info;
    ASSERT_EQ(0, stat(m_Path, &info));
    ASSERT_TRUE(S_ISDIR(info.st_mode));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
