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
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/testutil.h>
#include <dlib/zip.h>

static bool ReadFile(const char* relative_path, std::string* out)
{
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), relative_path);

    FILE* file = fopen(path, "rb");
    if (file == 0)
        return false;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    out->resize(file_size > 0 ? (size_t)file_size : 0);
    size_t read = 0;
    if (file_size > 0)
        read = fread(&(*out)[0], 1, out->size(), file);

    fclose(file);
    return read == out->size();
}

static const char* ExpectedNames[] = {
    "dir/",
    "hello.txt",
    "dir/data.bin",
    "empty.txt",
};

static const uint8_t ExpectedDataBin[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

struct ZipArchiveParams
{
    const char* m_Path;
    bool        m_UseStream;
};

class ZipArchiveTest : public jc_test_params_class<ZipArchiveParams>
{
protected:
    dmZip::HZip m_Zip;
    std::string m_Stream;

    virtual void SetUp()
    {
        m_Zip = 0;

        dmZip::Result zr;
        if (GetParam().m_UseStream)
        {
            ASSERT_TRUE(ReadFile(GetParam().m_Path, &m_Stream));
            zr = dmZip::OpenStream(m_Stream.data(), m_Stream.size(), &m_Zip);
        }
        else
        {
            char path[1024];
            dmTestUtil::MakeHostPath(path, sizeof(path), GetParam().m_Path);
            zr = dmZip::Open(path, &m_Zip);
        }

        ASSERT_EQ(dmZip::RESULT_OK, zr);
    }

    virtual void TearDown()
    {
        if (m_Zip)
            dmZip::Close(m_Zip);
        m_Zip = 0;
        m_Stream.clear();
    }
};

TEST(dmZip, NotExist)
{
    char path[128];
    dmTestUtil::MakeHostPath(path, sizeof(path), "NOTEEXIST");

    dmZip::HZip zip;
    dmZip::Result zr = dmZip::Open(path, &zip);
    ASSERT_NE(dmZip::RESULT_OK, zr);
}

TEST_P(ZipArchiveTest, Iterate)
{
    uint32_t num_entries = dmZip::GetNumEntries(m_Zip);
    ASSERT_EQ(4u, num_entries);

    uint32_t num_files = 0;
    uint32_t num_dirs = 0;

    for (uint32_t i = 0; i < num_entries; ++i)
    {
        dmZip::Result zr = dmZip::OpenEntry(m_Zip, i);
        ASSERT_EQ(dmZip::RESULT_OK, zr);

        if (dmZip::IsEntryDir(m_Zip))
            ++num_dirs;
        else
            ++num_files;

        const char* name = dmZip::GetEntryName(m_Zip);
        ASSERT_NE((const char*)0, name);
        ASSERT_STREQ(ExpectedNames[i], name);

        dmZip::CloseEntry(m_Zip);
    }

    ASSERT_EQ(3u, num_files);
    ASSERT_EQ(1u, num_dirs);
}

TEST_P(ZipArchiveTest, ReadByName)
{
    dmZip::Result zr = dmZip::OpenEntry(m_Zip, "hello.txt");
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_FALSE(dmZip::IsEntryDir(m_Zip));

    uint32_t size = 0;
    zr = dmZip::GetEntrySize(m_Zip, &size);
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_EQ(10u, size);

    std::string data;
    data.resize(size);
    zr = dmZip::GetEntryData(m_Zip, &data[0], size);
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_EQ(std::string("Hello Zip\n"), data);
    dmZip::CloseEntry(m_Zip);

    zr = dmZip::OpenEntry(m_Zip, "empty.txt");
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    zr = dmZip::GetEntrySize(m_Zip, &size);
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_EQ(0u, size);
    dmZip::CloseEntry(m_Zip);

    zr = dmZip::OpenEntry(m_Zip, "dir/");
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_TRUE(dmZip::IsEntryDir(m_Zip));
    dmZip::CloseEntry(m_Zip);
}

TEST_P(ZipArchiveTest, ReadPartial)
{
    dmZip::Result zr = dmZip::OpenEntry(m_Zip, "dir/data.bin");
    ASSERT_EQ(dmZip::RESULT_OK, zr);

    uint8_t buffer[3];
    uint32_t offset = 0;

    while (offset < sizeof(ExpectedDataBin))
    {
        uint32_t nread = 0;
        zr = dmZip::GetEntryDataOffset(m_Zip, offset, sizeof(buffer), buffer, &nread);
        ASSERT_EQ(dmZip::RESULT_OK, zr);
        ASSERT_NE(0u, nread);
        ASSERT_ARRAY_EQ_LEN(&ExpectedDataBin[offset], buffer, nread);
        offset += nread;
    }

    dmZip::CloseEntry(m_Zip);
}

const ZipArchiveParams params_zip_archives[] = {
    { "src/test/data/zip/archive_deflated.zip", false },
    { "src/test/data/zip/archive_stored.zip", false },
    { "src/test/data/zip/archive_deflated.zip", true },
    { "src/test/data/zip/archive_stored.zip", true },
};

INSTANTIATE_TEST_CASE_P(ZipArchiveOpenModes, ZipArchiveTest,
        jc_test_values_in(params_zip_archives));

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
