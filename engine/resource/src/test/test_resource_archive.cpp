#include <stdint.h>
#include <gtest/gtest.h>
#include "../resource_archive.h"

extern unsigned char TEST_ARC[];
extern uint32_t TEST_ARC_SIZE;
extern unsigned char TEST_COMPRESSED_ARC[];
extern uint32_t TEST_COMPRESSED_ARC_SIZE;

TEST(dmResourceArchive, Wrap)
{
    dmResourceArchive::HArchive archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer((void*) TEST_ARC, TEST_ARC_SIZE, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
    char* buf = new char[1024 * 1024];

    const char* names[] = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc", "/archive_data/file5.scriptc" };
    const char* data[] = { "file4_datafile4_datafile4_data", "file1_datafile1_datafile1_data", "file3_data", "file2_datafile2_datafile2_data", "stuff to test encryption" };
    dmResourceArchive::EntryInfo entry_info;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry(archive, names[i], &entry_info);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        dmResourceArchive::Read(archive, &entry_info, buf);
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }

    r = dmResourceArchive::FindEntry(archive, "does_not_exists", &entry_info);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
    delete [] buf;
}

TEST(dmResourceArchive, WrapCompressed)
{
    dmResourceArchive::HArchive archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer((void*) TEST_COMPRESSED_ARC, TEST_COMPRESSED_ARC_SIZE, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
    char* buf = new char[1024 * 1024];

    const char* names[] = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc", "/archive_data/file5.scriptc" };
    const char* data[] = { "file4_datafile4_datafile4_data", "file1_datafile1_datafile1_data", "file3_data", "file2_datafile2_datafile2_data", "stuff to test encryption" };
    dmResourceArchive::EntryInfo entry_info;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry(archive, names[i], &entry_info);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        dmResourceArchive::Read(archive, &entry_info, buf);
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }

    r = dmResourceArchive::FindEntry(archive, "does_not_exists", &entry_info);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
    delete [] buf;
}

TEST(dmResourceArchive, LoadFromDisk)
{
    dmResourceArchive::HArchive archive = 0;

    const char* archives[] = { "build/default/src/test/test.arc", "build/default/src/test/test.jarc" };

    for (int i = 0; i < 2; ++i)
    {
        dmResourceArchive::Result r = dmResourceArchive::LoadArchive(archives[i], &archive);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
        char* buf = new char[1024 * 1024];

        const char* names[] = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc", "/archive_data/file5.scriptc" };
        const char* data[] = { "file4_datafile4_datafile4_data", "file1_datafile1_datafile1_data", "file3_data", "file2_datafile2_datafile2_data", "stuff to test encryption" };
        dmResourceArchive::EntryInfo entry_info;
        for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        {
            r = dmResourceArchive::FindEntry(archive, names[i], &entry_info);
            ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
            dmResourceArchive::Read(archive, &entry_info, buf);
            ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
        }

        r = dmResourceArchive::FindEntry(archive, "does_not_exists", &entry_info);
        ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
        dmResourceArchive::Delete(archive);
        delete[] buf;
    }
}

TEST(dmResourceArchive, LoadFromDiskCompressed)
{
    dmResourceArchive::HArchive archive = 0;

    const char* archives[] = { "build/default/src/test/test_compressed.arc", "build/default/src/test/test_compressed.jarc" };

    for (int i = 0; i < 2; ++i)
    {
        dmResourceArchive::Result r = dmResourceArchive::LoadArchive(archives[i], &archive);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
        char* buf = new char[1024 * 1024];

        const char* names[] = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc", "/archive_data/file5.scriptc" };
        const char* data[] = { "file4_datafile4_datafile4_data", "file1_datafile1_datafile1_data", "file3_data", "file2_datafile2_datafile2_data", "stuff to test encryption" };
        dmResourceArchive::EntryInfo entry_info;
        for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        {
            r = dmResourceArchive::FindEntry(archive, names[i], &entry_info);
            ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
            dmResourceArchive::Read(archive, &entry_info, buf);
            ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
        }

        r = dmResourceArchive::FindEntry(archive, "does_not_exists", &entry_info);
        ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
        dmResourceArchive::Delete(archive);
        delete[] buf;
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
