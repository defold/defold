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
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/zip.h>
#include "zip/zip.h"

static uint32_t g_ZipReadCalls;
static uint64_t g_ZipReadBytes;

// Track the amount of backing-file I/O performed by a ZIP operation.
static size_t CountingReadFileAt(FILE* file, uint64_t offset, void* buffer, size_t size)
{
    if (dmSys::FileSeek64(file, offset) != 0)
        return 0;

    ++g_ZipReadCalls;
    size_t read = fread(buffer, 1, size, file);
    g_ZipReadBytes += read;
    return read;
}

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
    enum OpenMode
    {
        OPEN_FROM_PATH,
        OPEN_FROM_MEMORY,
        OPEN_FROM_FILE_RANGE,
        OPEN_FROM_RESOURCE,
    } m_OpenMode;
};

class ZipArchiveTest : public jc_test_params_class<ZipArchiveParams>
{
protected:
    dmZip::HZip m_Zip;
    FILE*        m_File;
    std::string m_Stream;

    virtual void SetUp()
    {
        m_Zip = 0;
        m_File = 0;

        dmZip::Result zr;
        if (GetParam().m_OpenMode == ZipArchiveParams::OPEN_FROM_MEMORY)
        {
            ASSERT_TRUE(ReadFile(GetParam().m_Path, &m_Stream));
            zr = dmZip::OpenStream(m_Stream.data(), m_Stream.size(), &m_Zip);
        }
        else
        {
            char path[1024];
            dmTestUtil::MakeHostPath(path, sizeof(path), GetParam().m_Path);
            if (GetParam().m_OpenMode == ZipArchiveParams::OPEN_FROM_FILE_RANGE)
            {
                m_File = fopen(path, "rb");
                ASSERT_NE((FILE*)0, m_File);
                ASSERT_EQ(0, fseek(m_File, 0, SEEK_END));
                long file_size = ftell(m_File);
                ASSERT_GT(file_size, 0);
                zr = dmZip::OpenFileRange(m_File, 0, (uint64_t)file_size, &m_Zip);
            }
            else if (GetParam().m_OpenMode == ZipArchiveParams::OPEN_FROM_RESOURCE)
            {
                zr = dmZip::OpenResource(path, &m_Zip);
            }
            else
            {
                zr = dmZip::Open(path, &m_Zip);
            }
        }

        ASSERT_EQ(dmZip::RESULT_OK, zr);
    }

    virtual void TearDown()
    {
        if (m_Zip)
            dmZip::Close(m_Zip);
        if (m_File)
            fclose(m_File);
        m_Zip = 0;
        m_File = 0;
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

// Verify that only the declared ZIP range is visible when unrelated data exists
// before and after it, and that closing the ZIP leaves the caller-owned file open.
TEST(dmZip, OpenFileRange)
{
    std::string archive;
    std::string trailing_archive;
    ASSERT_TRUE(ReadFile("src/test/data/zip/archive_deflated.zip", &archive));
    ASSERT_TRUE(ReadFile("src/test/data/zip/foo.zip", &trailing_archive));

    const char prefix[] = "not part of the zip";
    FILE*      file = tmpfile();
    ASSERT_NE((FILE*)0, file);
    ASSERT_EQ(sizeof(prefix) - 1, fwrite(prefix, 1, sizeof(prefix) - 1, file));
    ASSERT_EQ(archive.size(), fwrite(archive.data(), 1, archive.size(), file));
    ASSERT_EQ(trailing_archive.size(), fwrite(trailing_archive.data(), 1, trailing_archive.size(), file));
    ASSERT_EQ(0, fflush(file));

    dmZip::HZip   zip = 0;
    dmZip::Result zr = dmZip::OpenFileRange(file, sizeof(prefix) - 1, archive.size(), &zip);
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    ASSERT_EQ(4u, dmZip::GetNumEntries(zip));

    zr = dmZip::OpenEntry(zip, "hello.txt");
    ASSERT_EQ(dmZip::RESULT_OK, zr);
    uint32_t size = 0;
    ASSERT_EQ(dmZip::RESULT_OK, dmZip::GetEntrySize(zip, &size));
    ASSERT_EQ(10u, size);
    char data[10];
    ASSERT_EQ(dmZip::RESULT_OK, dmZip::GetEntryData(zip, data, sizeof(data)));
    ASSERT_ARRAY_EQ_LEN("Hello Zip\n", data, sizeof(data));
    dmZip::CloseEntry(zip);
    dmZip::Close(zip);

    // Closing the zip must not close the caller-owned file.
    ASSERT_EQ(0, fseek(file, 0, SEEK_SET));
    fclose(file);
}

// Verify that the range boundary is enforced and a ZIP truncated by that boundary
// is rejected without returning a partially initialized handle.
TEST(dmZip, OpenFileRangeRejectsTruncatedArchive)
{
    std::string archive;
    ASSERT_TRUE(ReadFile("src/test/data/zip/archive_stored.zip", &archive));

    FILE* file = tmpfile();
    ASSERT_NE((FILE*)0, file);
    ASSERT_EQ(archive.size(), fwrite(archive.data(), 1, archive.size(), file));
    ASSERT_EQ(0, fflush(file));

    dmZip::HZip   zip = 0;
    dmZip::Result zr = dmZip::OpenFileRange(file, 0, archive.size() - 1, &zip);
    ASSERT_NE(dmZip::RESULT_OK, zr);
    ASSERT_EQ((dmZip::HZip)0, zip);
    fclose(file);
}

// Verify that a partial read from a stored entry reads only its local header and
// requested bytes, including when the requested size would overflow offset + size.
TEST(dmZip, StoredPartialReadDoesNotScanFromStart)
{
    std::string archive;
    ASSERT_TRUE(ReadFile("src/test/data/zip/archive_stored.zip", &archive));

    const char prefix[] = "outside archive";
    FILE* file = tmpfile();
    ASSERT_NE((FILE*)0, file);
    ASSERT_EQ(0, setvbuf(file, 0, _IONBF, 0));
    ASSERT_EQ(sizeof(prefix) - 1, fwrite(prefix, 1, sizeof(prefix) - 1, file));
    ASSERT_EQ(archive.size(), fwrite(archive.data(), 1, archive.size(), file));

    zip_t* reader = zip_cstream_openwithoffset(file, sizeof(prefix) - 1,
                                                archive.size(), 9,
                                                CountingReadFileAt);
    ASSERT_NE((zip_t*)0, reader);
    ASSERT_EQ(0, zip_entry_open(reader, "dir/data.bin"));

    uint8_t buffer[2];
    const size_t offset = sizeof(ExpectedDataBin) - sizeof(buffer);
    g_ZipReadCalls = 0;
    g_ZipReadBytes = 0;
    ssize_t nread = zip_entry_noallocreadwithoffset(reader, offset,
                                                    (size_t)-1, buffer);
    ASSERT_EQ((ssize_t)sizeof(buffer), nread);
    ASSERT_ARRAY_EQ_LEN(ExpectedDataBin + offset, buffer, sizeof(buffer));

    // Clamp without overflowing offset + size, and read only the local header
    // and requested bytes. The iterator path also read and discarded all bytes
    // preceding the requested offset.
    ASSERT_EQ(2u, g_ZipReadCalls);
    const uint64_t zip_local_header_size = 30;
    ASSERT_EQ(zip_local_header_size + sizeof(buffer), g_ZipReadBytes);

    ASSERT_EQ(0, zip_entry_close(reader));
    zip_close(reader);
    fclose(file);
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
    // Preserve coverage of the existing filesystem and memory opening paths.
    { "src/test/data/zip/archive_deflated.zip", ZipArchiveParams::OPEN_FROM_PATH },
    { "src/test/data/zip/archive_stored.zip", ZipArchiveParams::OPEN_FROM_PATH },
    { "src/test/data/zip/archive_deflated.zip", ZipArchiveParams::OPEN_FROM_MEMORY },
    { "src/test/data/zip/archive_stored.zip", ZipArchiveParams::OPEN_FROM_MEMORY },
    // Run all parameterized ZIP operations through the new bounded-file path.
    { "src/test/data/zip/archive_deflated.zip", ZipArchiveParams::OPEN_FROM_FILE_RANGE },
    { "src/test/data/zip/archive_stored.zip", ZipArchiveParams::OPEN_FROM_FILE_RANGE },
    // Run the same operations through the platform resource backend.
    { "src/test/data/zip/archive_deflated.zip", ZipArchiveParams::OPEN_FROM_RESOURCE },
    { "src/test/data/zip/archive_stored.zip", ZipArchiveParams::OPEN_FROM_RESOURCE },
};

INSTANTIATE_TEST_CASE_P(ZipArchiveOpenModes, ZipArchiveTest,
        jc_test_values_in(params_zip_archives));

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
