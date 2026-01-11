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
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/dstrings.h>
#include <dlib/zip.h>
#include <dlib/testutil.h>

extern unsigned char FOO_ZIP[];
extern uint32_t FOO_ZIP_SIZE;

// If you get a duplicate here, then we've missed to put the "miniz.h" header within a namespace
extern "C" void mz_free(void *p)
{
    free(p);
}


#define PATH_FORMAT "src/test/data/%s"


TEST(dmZip, NotExist)
{
    char path[128];
    dmTestUtil::MakeHostPath(path, sizeof(path), "NOTEEXIST");

    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open(path, &zip);
    ASSERT_NE(dmZip::RESULT_OK, zr);
}

TEST(dmZip, Read)
{
    char path[128];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/foo.zip");

    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open(path, &zip);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    const char* entryname_1 = "no_such_file";
    zr = dmZip::OpenEntry(zip, entryname_1);
    ASSERT_EQ(dmZip::RESULT_NO_SUCH_ENTRY, zr);

    const char* entryname = "hello.txt";
    zr = dmZip::OpenEntry(zip, entryname);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    uint32_t manifest_len = 0;
    zr = dmZip::GetEntrySize(zip, &manifest_len);
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_EQ(11u, manifest_len);

    uint8_t* manifest_data = (uint8_t*)malloc(manifest_len+1);
    zr = dmZip::GetEntryData(zip, manifest_data, manifest_len);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    dmZip::CloseEntry(zip);

    manifest_data[manifest_len] = 0;

    ASSERT_STREQ("Hello World", (const char*)manifest_data);

    free(manifest_data);
    dmZip::Close(zip);
}

TEST(dmZip, ReadPartial)
{
    char path[128];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/foo.zip");

    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open(path, &zip);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    const char* entryname = "hello.txt";
    zr = dmZip::OpenEntry(zip, entryname);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    uint8_t expected_data[] = "Hello World";
    uint32_t expected_size = sizeof(expected_data)-1; // the \0 isn't stored in this file

    const uint32_t max_chunk_len = 11;
    uint8_t buffer[max_chunk_len];

    for (uint32_t i = 1; i <= max_chunk_len; ++i)
    {
        const uint32_t chunk_len = i;
        uint32_t offset = 0;

        while (offset < expected_size)
        {
            uint32_t nread;
            dmZip::GetEntryDataOffset(zip, offset, chunk_len, buffer, &nread);
            ASSERT_GE(chunk_len, nread);
            ASSERT_NE(0u, nread);

            ASSERT_ARRAY_EQ_LEN(&expected_data[offset], buffer, nread);

            offset += nread;
        }
    }

    dmZip::CloseEntry(zip);
    dmZip::Close(zip);
}

TEST(dmZip, Iterate)
{
    char path[128];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/foo.zip");

    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open(path, &zip);
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    uint32_t num_entries = dmZip::GetNumEntries(zip);
    ASSERT_EQ(3u, num_entries);

    uint32_t num_files = 0;
    uint32_t num_dirs = 0;
    uint32_t num_matches = 0;

    const char* names[] = {
            "dir/",
            "dir/data.bin",
            "hello.txt",
        };

    for( uint32_t i = 0; i < num_entries; ++i)
    {
        zr = dmZip::OpenEntry(zip, i);
        ASSERT_EQ(dmZip::RESULT_OK, zr);

        if (dmZip::IsEntryDir(zip))
            num_dirs++;
        else
            num_files++;

        const char* name = dmZip::GetEntryName(zip);
        ASSERT_NE((const char*)0, name);

        if (strcmp(names[i], name) == 0)
            num_matches++;

        dmZip::CloseEntry(zip);
    }

    ASSERT_EQ(2u, num_files);
    ASSERT_EQ(1u, num_dirs);
    ASSERT_EQ(3u, num_matches);

    dmZip::Close(zip);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
