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

#include <stdio.h>
#include <stdint.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>

#include "../providers/provider.h"
#include "../providers/provider_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// File generated with
// data = b"\x00\x01\x02\x03\x04\x05\x06\x07"
// with open('./src/test/files/somedata', 'wb') as f:
//     f.write(data)
const uint8_t SOMEDATA[8] = {0, 1, 2, 3, 4, 5, 6, 7};

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

TEST(FileProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(FileProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    dmURI::Parts uri;
    dmURI::Parse(".", &uri);
    ASSERT_TRUE(loader->m_CanMount(&uri));

    dmURI::Parse("dmanif:some/folder", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("http://domain.com/path", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));
}


class FileProviderArchive : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetCwd(m_WorkingDirectory, sizeof(m_WorkingDirectory)));
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        // MakeHostPath also initializes Emscripten's host filesystem.
        char path[DMPATH_MAX_PATH];
        dmTestUtil::MakeHostPath(path, sizeof(path), ".");
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(path));

        dmURI::Parts uri;
        dmURI::Parse(".", &uri);

        dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    void TearDown() override
    {
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_WorkingDirectory));
        dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmhash_t m_NameHashArchive;
    dmhash_t m_NameHashFile;

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::ArchiveLoader* m_Loader;
    char                               m_WorkingDirectory[DMPATH_MAX_PATH];
};


TEST_F(FileProviderArchive, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/empty", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(0U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/somedata", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(8U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/not_exist", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

TEST_F(FileProviderArchive, ReadFile)
{
    dmResourceProvider::Result result;
    uint8_t short_buffer[4];
    uint8_t long_buffer[64];

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/src/test/files/somedata", short_buffer, sizeof(short_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, result);

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/src/test/files/somedata", long_buffer, sizeof(long_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, long_buffer, sizeof(SOMEDATA));
}

TEST_F(FileProviderArchive, ReadFilePartial)
{
    dmResourceProvider::Result result;
    uint8_t long_buffer[64];

    uint32_t file_size;
    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/somedata", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_LE(file_size, (uint32_t)sizeof(long_buffer));

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/src/test/files/somedata", long_buffer, sizeof(long_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, long_buffer, sizeof(SOMEDATA));

    const uint32_t chunk_size = 5;
    uint8_t short_buffer[chunk_size];

    uint32_t offset = 0;
    while (offset < file_size)
    {
        uint32_t nread;
        result = dmResourceProvider::ReadFilePartial(m_Archive, 0, "/src/test/files/somedata", offset, chunk_size, short_buffer, &nread);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        ASSERT_GE(chunk_size, nread);

        ASSERT_ARRAY_EQ_LEN(&long_buffer[offset], short_buffer, nread);
        offset += nread;
    }
}

// Relative mounts must keep their root for size, full and partial reads after chdir.
TEST_F(FileProviderArchive, ReadAfterChangingWorkingDirectory)
{
    uint32_t file_size;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/somedata", &file_size));
    ASSERT_EQ((uint32_t)sizeof(SOMEDATA), file_size);

    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir("src/test/files"));

    EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/src/test/files/somedata", &file_size));
    EXPECT_EQ((uint32_t)sizeof(SOMEDATA), file_size);

    uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
    EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, "/src/test/files/somedata", buffer, sizeof(buffer)));
    EXPECT_ARRAY_EQ_LEN(SOMEDATA, buffer, sizeof(SOMEDATA));

    uint32_t nread = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/src/test/files/somedata", 2, 3, buffer, &nread));
    EXPECT_EQ(3U, nread);
    EXPECT_ARRAY_EQ_LEN(SOMEDATA + 2, buffer, 3);
}

// Expanding a relative mount must not truncate a long request into an existing filename.
TEST_F(FileProviderArchive, LongPathDoesNotReadTruncatedFile)
{
    char root[DMPATH_MAX_PATH];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(".", root, sizeof(root)));

    const char* file = "/src/test/files/somedata";
    uint32_t padding = DMPATH_MAX_PATH - 1 - (uint32_t)strlen(root) - (uint32_t)strlen(file);
    ASSERT_LT(padding, dmResource::RESOURCE_PATH_MAX);

    // Redundant separators put the existing filename at the old lookup buffer's boundary.
    char path[dmResource::RESOURCE_PATH_MAX];
    memset(path, '/', padding);
    ASSERT_LT(dmSnPrintf(path + padding, sizeof(path) - padding, "%sx", file), (int)(sizeof(path) - padding));

    uint32_t file_size = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::GetFileSize(m_Archive, 0, path, &file_size));
    uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFile(m_Archive, 0, path, buffer, sizeof(buffer)));
    uint32_t nread = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFilePartial(m_Archive, 0, path, 0, sizeof(buffer), buffer, &nread));
}

// An oversized resource path must not become a valid file after host-path normalization.
TEST_F(FileProviderArchive, OversizedPathDoesNotReadFile)
{
    const char* file = "/src/test/files/somedata";
    uint32_t padding = dmResource::RESOURCE_PATH_MAX - (uint32_t)strlen(file);
    char path[dmResource::RESOURCE_PATH_MAX + 1];
    memset(path, '/', padding);
    dmStrlCpy(path + padding, file, sizeof(path) - padding);

    uint32_t file_size = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::GetFileSize(m_Archive, 0, path, &file_size));
    uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFile(m_Archive, 0, path, buffer, sizeof(buffer)));
    uint32_t nread = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFilePartial(m_Archive, 0, path, 0, sizeof(buffer), buffer, &nread));
}

// A filesystem-root mount must preserve the system resolver's root and host fallback semantics.
TEST_F(FileProviderArchive, MountFileSystemRoot)
{
    char root[DMPATH_MAX_PATH];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath("/", root, sizeof(root)));

    const char* path = "/src/test/files/somedata";
    char full_path[DMPATH_MAX_PATH];
    dmSnPrintf(full_path, sizeof(full_path), "%s%s", root, path);
    char resolved_path[DMPATH_MAX_PATH];
    dmSys::Result resolved = dmSys::ResolveMountFileName(resolved_path, sizeof(resolved_path), full_path);
    uint32_t expected_size = 0;
    if (resolved == dmSys::RESULT_OK)
    {
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(resolved_path, &expected_size));
    }

    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
    char uri_string[DMPATH_MAX_PATH];
    dmSnPrintf(uri_string, sizeof(uri_string), "file:%s", root);
    dmURI::Parts uri;
    dmURI::Parse(uri_string, &uri);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive));

    uint32_t actual_size = 0;
    dmResourceProvider::Result expected = resolved == dmSys::RESULT_OK ? dmResourceProvider::RESULT_OK : dmResourceProvider::RESULT_NOT_FOUND;
    EXPECT_EQ(expected, dmResourceProvider::GetFileSize(m_Archive, 0, path, &actual_size));
    EXPECT_EQ(expected_size, actual_size);
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    return result;
}
