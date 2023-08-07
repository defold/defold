// Copyright 2020-2023 The Defold Foundation
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
#include "../resource.h"
#include "../resource_archive.h"
#include "../resource_mounts.h"
#include "../resource_mounts_file.h"
#include "../resource_private.h"
#include "../providers/provider.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <testmain/testmain.h>

const char* MOUNTFILE_PATH = "liveupdate.mounts";


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static bool WriteMountFile(const char* path, int version, const dmArray<dmResourceMounts::MountFileEntry>& entries)
{
    FILE* file = fopen(path, "wb");
    if (!file)
    {
        printf("Failed to open %s for writing\n", path);
        return false;
    }

    char buffer[1024];
    uint32_t size = dmSnPrintf(buffer, sizeof(buffer), "VERSION%s%d\n", dmResourceMounts::MOUNTSFILE_CSV_SEPARATOR, version);
    fwrite(buffer, size, 1, file);

    uint32_t count = entries.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        const dmResourceMounts::MountFileEntry& entry = entries[i];
        uint32_t size = dmSnPrintf(buffer, sizeof(buffer), "MOUNT%s%d%s%s%s%s\n",
                dmResourceMounts::MOUNTSFILE_CSV_SEPARATOR, entry.m_Priority,
                dmResourceMounts::MOUNTSFILE_CSV_SEPARATOR, entry.m_Name,
                dmResourceMounts::MOUNTSFILE_CSV_SEPARATOR, entry.m_Uri);
        fwrite(buffer, size, 1, file);
    }
    fclose(file);
    return true;
}


TEST(MountsFile, WriteMounts)
{
    dmArray<dmResourceMounts::MountFileEntry> entries;
    entries.SetCapacity(8);

    dmResourceMounts::MountFileEntry entry;

    entry.m_Name = strdup("a");
    entry.m_Uri = strdup("uri_a");
    entry.m_Priority = 2;
    entries.Push(entry);

    entry.m_Name = strdup("b");
    entry.m_Uri = strdup("uri_b");
    entry.m_Priority = 1;
    entries.Push(entry);

    entry.m_Name = strdup("c");
    entry.m_Uri = strdup("uri_c");
    entry.m_Priority = 3;
    entries.Push(entry);

    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);
    dmResource::Result result = dmResourceMounts::WriteMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    dmResourceMounts::FreeMountsFile(entries);
}

TEST(MountsFile, ReadMounts)
{
    dmArray<dmResourceMounts::MountFileEntry> entries;

    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);

    dmResource::Result result = dmResourceMounts::ReadMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(3u, entries.Size());

    {
        const dmResourceMounts::MountFileEntry& entry = entries[0];
        ASSERT_STREQ("a", entry.m_Name);
        ASSERT_STREQ("uri_a", entry.m_Uri);
        ASSERT_EQ(2, entry.m_Priority);
    }

    {
        const dmResourceMounts::MountFileEntry& entry = entries[1];
        ASSERT_STREQ("b", entry.m_Name);
        ASSERT_STREQ("uri_b", entry.m_Uri);
        ASSERT_EQ(1, entry.m_Priority);
    }

    {
        const dmResourceMounts::MountFileEntry& entry = entries[2];
        ASSERT_STREQ("c", entry.m_Name);
        ASSERT_STREQ("uri_c", entry.m_Uri);
        ASSERT_EQ(3, entry.m_Priority);
    }

    printf("FreeMountsFile\n");

    dmResourceMounts::FreeMountsFile(entries);
}

TEST(MountsFile, ReadBadHeader)
{
    // Write bad header
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);
    dmArray<dmResourceMounts::MountFileEntry> entries;
    ASSERT_TRUE(WriteMountFile(path, -1, entries));

    dmResource::Result result = dmResourceMounts::ReadMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_VERSION_MISMATCH, result);
    ASSERT_EQ(0u, entries.Size());
    dmResourceMounts::FreeMountsFile(entries);
}

TEST(MountsFile, ReadBadEntry)
{
    // Write bad header
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);

    dmArray<dmResourceMounts::MountFileEntry> entries;
    entries.SetCapacity(8);

    dmResourceMounts::MountFileEntry entry;
    entry.m_Priority = 0;
    entry.m_Name = strdup("empty");
    entry.m_Uri = 0;
    entries.Push(entry);

    dmResource::Result result = dmResourceMounts::WriteMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    dmResourceMounts::FreeMountsFile(entries);
    entries.SetSize(0);

    result = dmResourceMounts::ReadMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(0u, entries.Size());
    dmResourceMounts::FreeMountsFile(entries);
}

TEST(MountsFile, ReadFileWithSpaces)
{
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/mountfiles/spaces.mounts");

    dmArray<dmResourceMounts::MountFileEntry> entries;
    dmResource::Result result = dmResourceMounts::ReadMountsFile(path, entries);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(1u, entries.Size());

    const dmResourceMounts::MountFileEntry entry0 = entries[0];
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(10, entry0.m_Priority);
    ASSERT_STREQ("_liveupdate", entry0.m_Name);
    ASSERT_STREQ("zip:/Users/mawe/Library/Application Support/LiveUpdateDemo/defold.resourcepack.zip", entry0.m_Uri);

    dmResourceMounts::FreeMountsFile(entries);
}


class ArchiveProvidersMounts : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        dmArray<dmResourceMounts::MountFileEntry> entries;
        entries.SetCapacity(8);

        dmResourceMounts::MountFileEntry entry;

        entry.m_Name = strdup("a");
        entry.m_Uri = strdup("build/src/test/overrides");
        entry.m_Priority = 30;
        entries.Push(entry);

        entry.m_Name = strdup("b");
        entry.m_Uri = strdup("dmanif:build/src/test/luresources"); // unknown scheme
        entry.m_Priority = 10;
        entries.Push(entry);

        entry.m_Name = strdup("c");
        entry.m_Uri = strdup("zip:build/src/test/luresources_compressed.zip");
        entry.m_Priority = 20;
        entries.Push(entry);

        entry.m_Name = strdup("d");
        entry.m_Uri = strdup("archive:build/src/test/luresources");
        entry.m_Priority = 5;
        entries.Push(entry);

        char path[1024];
        dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);

        dmResource::Result result = dmResourceMounts::WriteMountsFile(path, entries);
        dmResourceMounts::FreeMountsFile(entries);
        ASSERT_EQ(dmResource::RESULT_OK, result);

        m_Mounts = dmResourceMounts::Create(0);
        ASSERT_NE((dmResourceMounts::HContext)0, m_Mounts);

        dmSnPrintf(path, sizeof(path), "%s", DM_HOSTFS);

        result = dmResourceMounts::LoadMounts(m_Mounts, path);
    }

    virtual void TearDown()
    {
        dmResourceMounts::Destroy(m_Mounts);

        char path[1024];
        dmTestUtil::MakeHostPath(path, sizeof(path), MOUNTFILE_PATH);
        dmSys::Unlink(path);
    }

    dmResourceProvider::Result Mount(dmResourceProvider::ArchiveLoader* loader, const char* path, dmResourceProvider::HArchive* archive)
    {
        dmURI::Parts uri;
        dmURI::Parse(path, &uri);
        return dmResourceProvider::CreateMount(loader, &uri, 0, archive);
    }

    dmResourceMounts::HContext   m_Mounts;
    dmResourceProvider::HArchive m_Archives[3];
};

TEST_F(ArchiveProvidersMounts, LoadMounts)
{
    dmResourceMounts::SGetMountResult info;
    dmResource::Result result;

    ASSERT_EQ(3, dmResourceMounts::GetNumMounts(m_Mounts));

    result = dmResourceMounts::GetMountByIndex(m_Mounts, 0, &info);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(30, info.m_Priority);
    ASSERT_STREQ("a", info.m_Name);

    result = dmResourceMounts::GetMountByIndex(m_Mounts, 1, &info);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(20, info.m_Priority);
    ASSERT_STREQ("c", info.m_Name);

    result = dmResourceMounts::GetMountByIndex(m_Mounts, 2, &info);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(5, info.m_Priority);
    ASSERT_STREQ("d", info.m_Name);
}

TEST_F(ArchiveProvidersMounts, SaveAndLoad)
{
    char path[1024];
    dmSnPrintf(path, sizeof(path), "%s", DM_HOSTFS);
    dmResource::Result result;

    dmResourceMounts::SGetMountResult mount_info;
    result = dmResourceMounts::GetMountByName(m_Mounts, "c", &mount_info);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    result = dmResourceMounts::RemoveMount(m_Mounts, mount_info.m_Archive);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    ASSERT_EQ(2, dmResourceMounts::GetNumMounts(m_Mounts));

    result = dmResourceMounts::SaveMounts(m_Mounts, path);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    // Unmount all mounts, recreate the system
    dmResourceMounts::Destroy(m_Mounts);
    m_Mounts = dmResourceMounts::Create(0);
    ASSERT_NE((dmResourceMounts::HContext)0, m_Mounts);

    result = dmResourceMounts::LoadMounts(m_Mounts, path);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    ASSERT_EQ(2, dmResourceMounts::GetNumMounts(m_Mounts));
}

int main(int argc, char **argv)
{
    TestMainPlatformInit();

    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
