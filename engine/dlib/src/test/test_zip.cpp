// Copyright 2020 The Defold Foundation
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
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/dstrings.h>
#include <dlib/zip.h>

extern unsigned char FOO_ZIP[];
extern uint32_t FOO_ZIP_SIZE;


#if defined(__NX__)
    #define MOUNTFS "host:/"
#else
    #define MOUNTFS ""
#endif

#define PATH_FORMAT "src/test/data/%s"


TEST(dmZip, NotExist)
{
    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open("FOOBAR", &zip);
    ASSERT_NE(dmZip::RESULT_OK, zr);
}

TEST(dmZip, Open)
{
    char path[64];
    dmSnPrintf(path, 64, MOUNTFS PATH_FORMAT, "foo.zip");

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

    dmZip::Close(zip);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
