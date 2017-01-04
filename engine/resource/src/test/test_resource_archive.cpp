#include <stdint.h>
#include <gtest/gtest.h>
#include "../resource_archive.h"

extern unsigned char TEST_ARC[];
extern uint32_t TEST_ARC_SIZE;
extern unsigned char TEST_COMPRESSED_ARC[];
extern uint32_t TEST_COMPRESSED_ARC_SIZE;

// new file format, generated test data
extern unsigned char TEST_ARCI[];
extern uint32_t TEST_ARCI_SIZE;
extern unsigned char TEST_ARCD[];
extern uint32_t TEST_ARCD_SIZE;
extern unsigned char TEST_COMPRESSED_ARCI[];
extern uint32_t TEST_COMPRESSED_ARCI_SIZE;
extern unsigned char TEST_COMPRESSED_ARCD[];
extern uint32_t TEST_COMPRESSED_ARCD_SIZE;

static const char* hashes[] = { "awesome hash here2", "awesome hash here5", "awesome hash here3", "awesome hash here4", "awesome hash here1" };
static const char* hash_not_found = "awesome hash NOT here";
static const char* names[] = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc", "/archive_data/file5.scriptc" };
static const char* data[] = { "file4_datafile4_datafile4_data", "file1_datafile1_datafile1_data", "file3_data", "file2_datafile2_datafile2_data", "stuff to test encryption" };

TEST(dmResourceArchive, Wrap2)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer2((void*) TEST_ARCI, TEST_ARCI_SIZE, TEST_ARCD, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount2(archive));
    char* buf = new char[1024 * 1024];

    dmResourceArchive::EntryData e;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry2(archive, (uint8_t*)hashes[i], &e);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        r = dmResourceArchive::Read2(archive, &e, buf);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }

    r = dmResourceArchive::FindEntry2(archive, (uint8_t*) hash_not_found, &e);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
    delete [] buf;
}

TEST(dmResourceArchive, Wrap)
{
    dmResourceArchive::HArchive archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer((void*) TEST_ARC, TEST_ARC_SIZE, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
    char* buf = new char[1024 * 1024];

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

TEST(dmResourceArchive, WrapCompressed2)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer2((void*) TEST_COMPRESSED_ARCI, TEST_COMPRESSED_ARCI_SIZE, (void*) TEST_COMPRESSED_ARCD, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount2(archive));
    char* buf = new char[1024 * 1024];

    dmResourceArchive::EntryData e;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry2(archive, (uint8_t*)hashes[i], &e);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        dmResourceArchive::Read2(archive, &e, buf);
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }

    r = dmResourceArchive::FindEntry2(archive, (uint8_t*) hash_not_found, &e);
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

TEST(dmResourceArchive, LoadFromDisk2)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;

    const char* archives[] = { "build/default/src/test/test.arci" };

    dmResourceArchive::Result r = dmResourceArchive::LoadArchive2(archives[0], &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5u, dmResourceArchive::GetEntryCount2(archive));
    char* buf = new char[1024 * 1024];

    dmResourceArchive::EntryData e;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry2(archive, (uint8_t*)hashes[i], &e);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, dmResourceArchive::Read2(archive, &e, buf));
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }

    r = dmResourceArchive::FindEntry2(archive, (uint8_t*) hash_not_found, &e);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
    dmResourceArchive::Delete2(archive);
    delete[] buf;
}

TEST(dmResourceArchive, LoadFromDisk)
{
    dmResourceArchive::HArchive archive = 0;

    const char* archives[] = { "build/default/src/test/test.arc" }; //, "build/default/src/test/test.jarc" };

    for (int i = 0; i < 2; ++i)
    {
        dmResourceArchive::Result r = dmResourceArchive::LoadArchive(archives[i], &archive);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
        char* buf = new char[1024 * 1024];
        
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

TEST(dmResourceArchive, LoadNonExistentArchiveFromDisk2)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;

    const char* archives[] = { "build/default/src/test/this-file-does-not-exist.arci" };

    dmResourceArchive::Result r = dmResourceArchive::LoadArchive2(archives[0], &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_IO_ERROR, r);
}

TEST(dmResourceArchive, LoadFromDiskCompressed2)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;

    const char* archives[] = { "build/default/src/test/test_compressed.arci" };

    dmResourceArchive::Result r = dmResourceArchive::LoadArchive2(archives[0], &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
    ASSERT_EQ(5u, dmResourceArchive::GetEntryCount2(archive));
    char* buf = new char[1024 * 1024];
    
    dmResourceArchive::EntryData e;
    for (uint32_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        r = dmResourceArchive::FindEntry2(archive, (uint8_t*)hashes[i], &e);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, dmResourceArchive::Read2(archive, &e, buf));
        ASSERT_TRUE(strncmp(data[i], (const char*) buf, strlen(data[i])) == 0);
    }
    
    r = dmResourceArchive::FindEntry2(archive, (uint8_t*) hash_not_found, &e);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, r);
    dmResourceArchive::Delete2(archive);
    delete[] buf;
}

TEST(dmResourceArchive, LoadFromDiskCompressed)
{
    dmResourceArchive::HArchive archive = 0;

    const char* archives[] = { "build/default/src/test/test_compressed.arc"};//, "build/default/src/test/test_compressed.jarc" };

    for (int i = 0; i < 2; ++i)
    {
        dmResourceArchive::Result r = dmResourceArchive::LoadArchive(archives[i], &archive);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, r);
        ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));
        char* buf = new char[1024 * 1024];

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
