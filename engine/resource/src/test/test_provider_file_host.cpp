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

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/sys_private.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>

#include "../providers/provider.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static char g_HostRoot[DMPATH_MAX_PATH];
static const uint8_t SOMEDATA[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static const char* MakePathTruncatingToHostFile(const char* root, char* path, uint32_t path_size)
{
    // Select an existing filename so the remaining bytes can be filled by "/." components.
    const char* file = (strlen(root) & 1) ? "/empty" : "/somedata";
    uint32_t padding = DMPATH_MAX_PATH - 1 - (uint32_t)strlen(root) - (uint32_t)strlen(file);
    if (padding >= path_size)
    {
        return 0;
    }
    for (uint32_t i = 0; i < padding; i += 2)
    {
        path[i] = '/';
        path[i + 1] = '.';
    }
    if (dmSnPrintf(path + padding, path_size - padding, "%sx", file) >= (int)(path_size - padding))
    {
        return 0;
    }
    return file;
}

namespace dmSys
{
    // Replace the provider's host mapper while keeping production path expansion and filesystem reads.
    Result GetTestHostFileName(char* buffer, size_t buffer_size, const char* path)
    {
        if (!g_HostRoot[0])
        {
            return RESULT_NOENT;
        }
        return GetHostFileNameImpl(buffer, buffer_size, path, g_HostRoot);
    }
}

class HostFileProviderArchive : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Archive = 0;
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetCwd(m_WorkingDirectory, sizeof(m_WorkingDirectory)));

        char path[DMPATH_MAX_PATH];
        dmTestUtil::MakeHostPath(path, sizeof(path), ".");
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(path));

        dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/files");
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(path, g_HostRoot, sizeof(g_HostRoot)));
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetCwd(path, sizeof(path)));
        ASSERT_STRNE(path, g_HostRoot);
        ASSERT_FALSE(dmSys::ResourceExists("somedata"));

        dmTestUtil::MakeHostPath(path, sizeof(path), "src/test");
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(path, m_OtherDirectory, sizeof(m_OtherDirectory)));

        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
        ASSERT_NE((dmResourceProvider::HArchiveLoader)0, m_Loader);
        dmURI::Parts uri;
        dmURI::Parse(".", &uri);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive));
    }

    void TearDown() override
    {
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_WorkingDirectory));
        if (m_Archive)
        {
            ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
        }
        g_HostRoot[0] = 0;
    }

    void CheckReads(const char* path)
    {
        uint32_t file_size = 0;
        EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, path, &file_size));
        EXPECT_EQ((uint32_t)sizeof(SOMEDATA), file_size);

        uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
        EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, path, buffer, sizeof(buffer)));
        EXPECT_ARRAY_EQ_LEN(SOMEDATA, buffer, sizeof(SOMEDATA));

        uint32_t nread = 0;
        EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, path, 2, 3, buffer, &nread));
        EXPECT_EQ(3U, nread);
        EXPECT_ARRAY_EQ_LEN(SOMEDATA + 2, buffer, 3);
    }

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::HArchiveLoader m_Loader;
    char                               m_WorkingDirectory[DMPATH_MAX_PATH];
    char                               m_OtherDirectory[DMPATH_MAX_PATH];
};

// Host fallback must work from outside the host root and survive later cwd changes.
TEST_F(HostFileProviderArchive, HostFallbackSurvivesWorkingDirectoryChange)
{
    CheckReads("/somedata");
    CheckReads("\\somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_OtherDirectory));
    CheckReads("/somedata");
    CheckReads("\\somedata");
}

// Native files must keep priority over different host files at the same resource path.
TEST_F(HostFileProviderArchive, NativeFilesKeepPriority)
{
    CheckReads("/src/test/files/somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_OtherDirectory));
    CheckReads("/src/test/files/somedata");
}

// Host-root expansion must not truncate a long request into an existing host filename.
TEST_F(HostFileProviderArchive, LongPathDoesNotReadTruncatedHostFile)
{
    char mapped_root[DMPATH_MAX_PATH];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetTestHostFileName(mapped_root, sizeof(mapped_root), "./."));
    char root[DMPATH_MAX_PATH];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(mapped_root, root, sizeof(root)));

    char path[dmResource::RESOURCE_PATH_MAX];
    const char* file = MakePathTruncatingToHostFile(root, path, sizeof(path));
    ASSERT_NE((const char*)0, file);
    char truncated_target[DMPATH_MAX_PATH];
    ASSERT_LT(dmSnPrintf(truncated_target, sizeof(truncated_target), "%s%s", root, file), (int)sizeof(truncated_target));
    ASSERT_TRUE(dmSys::ResourceExists(truncated_target));

    uint32_t file_size = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::GetFileSize(m_Archive, 0, path, &file_size));
    uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFile(m_Archive, 0, path, buffer, sizeof(buffer)));
    uint32_t nread = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFilePartial(m_Archive, 0, path, 0, sizeof(buffer), buffer, &nread));
}

// A truncated host root must not cache a different directory that contains a real file.
TEST_F(HostFileProviderArchive, TruncatedHostRootDoesNotReadOtherDirectory)
{
    const char* directory = "src/test/files";
    uint32_t padding = DMPATH_MAX_PATH - 2 - (uint32_t)strlen(g_HostRoot) - (uint32_t)strlen(directory);
    ASSERT_GT(padding, 1U);
    char root[DMPATH_MAX_PATH];
    root[0] = '.';
    memset(root + 1, '/', padding - 1);
    ASSERT_LT(dmSnPrintf(root + padding, sizeof(root) - padding, "%s-missing", directory), (int)(sizeof(root) - padding));

    char absolute_root[DMPATH_MAX_PATH];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(root, absolute_root, sizeof(absolute_root)));
    ASSERT_FALSE(dmSys::ResourceExists(absolute_root));

    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
    m_Archive = 0;
    dmURI::Parts uri;
    dmURI::Parse(root, &uri);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive));

    uint32_t file_size = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::GetFileSize(m_Archive, 0, "/somedata", &file_size));
    uint8_t buffer[sizeof(SOMEDATA)] = { 0 };
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", buffer, sizeof(buffer)));
    uint32_t nread = 0;
    EXPECT_EQ(dmResourceProvider::RESULT_NOT_FOUND, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/somedata", 0, sizeof(buffer), buffer, &nread));
}

// Failure to resolve a host fallback must not break an otherwise valid native mount.
TEST_F(HostFileProviderArchive, UnavailableHostKeepsNativeMount)
{
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
    m_Archive = 0;
    g_HostRoot[0] = 0;

    dmURI::Parts uri;
    dmURI::Parse(".", &uri);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive));
    CheckReads("/src/test/files/somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_OtherDirectory));
    CheckReads("/src/test/files/somedata");
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
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
