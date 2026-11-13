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

// Only the provider uses the simulated package backend; backing-file reads use dmSys.
#undef ResourceExistsInPackage
#undef ResourceExists
#undef ResourceSize
#undef LoadResource
#undef LoadResourcePartial

#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>

#include "../providers/provider.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static char g_PackageFile[DMPATH_MAX_PATH];
static uint32_t g_RelativeFileLookups;
static const uint8_t SOMEDATA[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static bool IsPackageFile(const char* path)
{
    if (path[0] == '/')
    {
        return false;
    }
    if (strncmp(path, "./", 2) == 0)
    {
        path += 2;
    }
    while (*path == '/')
    {
        ++path;
    }
    return strcmp(path, "asset-dir/somedata") == 0 || strcmp(path, "somedata") == 0;
}

static const char* ResolveTestFile(const char* path)
{
    if (IsPackageFile(path))
    {
        return g_PackageFile;
    }
    if (path[0] != '/' && !(path[0] && path[1] == ':'))
    {
        ++g_RelativeFileLookups;
    }
    return path;
}

namespace dmSys
{
    bool GetTestResourceExistsInPackage(const char* path)
    {
        return IsPackageFile(path);
    }

    bool GetTestResourceExists(const char* path)
    {
        return ResourceExists(ResolveTestFile(path));
    }

    Result GetTestResourceSize(const char* path, uint32_t* resource_size)
    {
        return ResourceSize(ResolveTestFile(path), resource_size);
    }

    Result GetTestLoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        return LoadResource(ResolveTestFile(path), buffer, buffer_size, resource_size);
    }

    Result GetTestLoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        return LoadResourcePartial(ResolveTestFile(path), offset, size, buffer, nread);
    }
}

class AssetFileProviderArchive : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Archive = 0;
        g_RelativeFileLookups = 0;
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetCwd(m_WorkingDirectory, sizeof(m_WorkingDirectory)));
        char path[DMPATH_MAX_PATH];
        dmTestUtil::MakeHostPath(path, sizeof(path), ".");
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir(path));
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath("src/test/files/somedata", g_PackageFile, sizeof(g_PackageFile)));
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
        ASSERT_NE((dmResourceProvider::HArchiveLoader)0, m_Loader);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, Remount("asset-dir"));
    }

    void TearDown() override
    {
        EXPECT_EQ(dmSys::RESULT_OK, dmSys::ChDir(m_WorkingDirectory));
        if (m_Archive)
        {
            EXPECT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
        }
    }

    dmResourceProvider::Result Remount(const char* root)
    {
        if (m_Archive)
        {
            dmResourceProvider::Unmount(m_Archive);
            m_Archive = 0;
        }
        dmURI::Parts uri;
        dmURI::Parse(root, &uri);
        return dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive);
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
        EXPECT_EQ(0U, g_RelativeFileLookups);
    }

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::HArchiveLoader m_Loader;
    char                               m_WorkingDirectory[DMPATH_MAX_PATH];
};

// Packaged assets must retain asset-relative names before and after process cwd changes.
TEST_F(AssetFileProviderArchive, PackagedReadsSurviveWorkingDirectoryChanges)
{
    CheckReads("/somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir("src/test/files"));
    CheckReads("/somedata");
}

// A dot mount must also support assets stored at the package root.
TEST_F(AssetFileProviderArchive, DotMountReadsPackageRoot)
{
    ASSERT_EQ(dmResourceProvider::RESULT_OK, Remount("."));
    CheckReads("/somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir("src/test/files"));
    CheckReads("/somedata");
}

// Loose files must stay anchored even when the same backend can also read packaged assets.
TEST_F(AssetFileProviderArchive, FilesystemReadsSurviveWorkingDirectoryChanges)
{
    ASSERT_EQ(dmResourceProvider::RESULT_OK, Remount("."));
    CheckReads("/src/test/files/somedata");
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ChDir("src/test/files"));
    CheckReads("/src/test/files/somedata");
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
