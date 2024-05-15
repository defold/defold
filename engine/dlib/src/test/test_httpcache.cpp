// Copyright 2020-2024 The Defold Foundation
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
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/http_cache.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <dlib/hash.h>
#include <dlib/dstrings.h>
#include <dlib/testutil.h>

class dmHttpCacheTest : public jc_test_base_class
{
    virtual void SetUp()
    {
        char path[1024];
        dmTestUtil::MakeHostPath(path, sizeof(path), "tmp");
        dmSys::Mkdir(path, 0755);

        dmTestUtil::MakeHostPath(path, sizeof(path), "tmp/afile");
        FILE* f = fopen(path, "wb");
        fclose(f);

        dmTestUtil::MakeHostPath(m_Path, sizeof(m_Path), "tmp/cache");
        dmSys::RmTree(m_Path);
    }

public:
    dmHttpCache::Result Put(dmHttpCache::HCache cache, const char* uri, const char* etag, const void* content, uint32_t content_len)
    {
        dmHttpCache::Result r;
        dmHttpCache::HCacheCreator cache_creator;
        r = dmHttpCache::Begin(cache, uri, etag, 0, &cache_creator);
        if (r != dmHttpCache::RESULT_OK)
            return r;
        r = dmHttpCache::Add(cache, cache_creator, content, content_len);
        if (r != dmHttpCache::RESULT_OK)
            return r;
        r = dmHttpCache::End(cache, cache_creator);
        return r;
    }

    dmHttpCache::Result Put(dmHttpCache::HCache cache, const char* uri, uint32_t max_age, const void* content, uint32_t content_len)
    {
        dmHttpCache::Result r;
        dmHttpCache::HCacheCreator cache_creator;
        r = dmHttpCache::Begin(cache, uri, "", max_age, &cache_creator);
        if (r != dmHttpCache::RESULT_OK)
            return r;
        r = dmHttpCache::Add(cache, cache_creator, content, content_len);
        if (r != dmHttpCache::RESULT_OK)
            return r;
        r = dmHttpCache::End(cache, cache_creator);
        return r;
    }

    dmHttpCache::Result Get(dmHttpCache::HCache cache, const char* uri, const char* etag, void** content, uint64_t* checksum, uint32_t* size_out = 0)
    {
        FILE* f = 0;
        dmHttpCache::Result r;
        r = dmHttpCache::Get(cache, uri, etag, &f, 0, checksum);
        if (r != dmHttpCache::RESULT_OK)
            return r;

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void* buffer = malloc(size);
        size_t n_read = fread(buffer, 1, size, f);
        // ... not ASSERT_EQ here due to
        // ...assertions that generate a fatal failure (FAIL* and ASSERT_*) can only be used in void-returning functions...
        // http://code.google.com/p/googletest/wiki/AdvancedGuide
        EXPECT_EQ(size, n_read);
        *content = buffer;
        if (size_out)
            *size_out = size;
        dmHttpCache::Release(cache, uri, etag, f);

        return dmHttpCache::RESULT_OK;
    }

    char m_Path[1024];
};

TEST_F(dmHttpCacheTest, NewCache)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, PathIsFile)
{
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), "tmp/afile");
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_INVALID_PATH, r);
}

TEST_F(dmHttpCacheTest, CorruptIndex)
{
    char index_path[1024];
    dmSnPrintf(index_path, sizeof(index_path), "%s/index", m_Path);

    dmSys::Mkdir(m_Path, 0755);
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    FILE* f = fopen(index_path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fclose(f);
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, Simple)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri", "etag", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etags
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag", tag_buffer);

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    r = Get(cache, "uri", "etag", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, SetError)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Start cache entry creation
    dmHttpCache::HCacheCreator cache_creator;
    r = dmHttpCache::Begin(cache, "uri", "etag", 0, &cache_creator);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = dmHttpCache::Add(cache, cache_creator, "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    // Raise an error
    dmHttpCache::SetError(cache, cache_creator);
    // Finish cache entry creation, which due to the error will not generate
    // a cache entry
    r = dmHttpCache::End(cache, cache_creator);
    ASSERT_EQ(dmHttpCache::RESULT_IO_ERROR, r);

    // Check that there is no stored etag
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, MaxAge)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    uint32_t max_age = 10U;

    r = Put(cache, "uri", max_age, "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etag
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_NO_ETAG, r);

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    r = Get(cache, "uri", "", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    dmHttpCache::EntryInfo info;
    r = dmHttpCache::GetInfo(cache, "uri", &info);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(0U, info.m_Verified);
    ASSERT_EQ(1U, info.m_Valid);
    ASSERT_NEAR(info.m_Expires, (uint64_t) (dmTime::GetTime() + max_age * 1000000U), (uint64_t) 1U * 1000000U);

    r = Put(cache, "uri", 1U, "data", strlen("data"));
    dmTime::Sleep(1000000U);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = dmHttpCache::GetInfo(cache, "uri", &info);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(0U, info.m_Verified);
    ASSERT_EQ(0U, info.m_Valid);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

static void CorruptFile(void*, const char* path, bool isdir)
{
    if (isdir)
        return;

    FILE* f = fopen(path, "wb");
    if (!f) {
        dmLogError("Failed to open '%s' for writing", path);
        return;
    }

    const char* str = "CORRUPTED_FILE";
    fwrite(str, 1, strlen(str), f);
    fclose(f);
}

static void CorruptDirs(void* ctx, const char* path, bool isdir)
{
    const char* basename = strrchr(path, '/');
    if (isdir && basename && strlen(basename+1) == 2)
    {
        dmSys::IterateTree(path, false, false, ctx, CorruptFile);
    }
}

TEST_F(dmHttpCacheTest, CorruptContent)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri", "etag", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etags
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag", tag_buffer);

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    uint32_t size;
    r = Get(cache, "uri", "etag", &buffer, &checksum, &size);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_EQ(checksum, dmHashBuffer64(buffer, size));
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    ASSERT_EQ(1U, dmHttpCache::GetEntryCount(cache));

    dmSys::IterateTree(m_Path, false, false, 0, CorruptDirs);

    // Get content, ensure that the checksum is incorrect
    r = Get(cache, "uri", "etag", &buffer, &checksum, &size);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_NE(checksum, dmHashBuffer64(buffer, size));
    ASSERT_EQ(1U, dmHttpCache::GetEntryCount(cache));
    free(buffer);

    // Close
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

static void MissingContent_File(void*, const char* path, bool isdir)
{
    if (isdir)
        return;
    dmSys::Unlink(path);
}

static void MissingContent_Dirs(void* ctx, const char* path, bool isdir)
{
    const char* basename = strrchr(path, '/');
    if (isdir && basename && strlen(basename+1) == 2)
    {
        dmSys::IterateTree(path, false, false, ctx, MissingContent_File);
    }
}

TEST_F(dmHttpCacheTest, MissingContent)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri", "etag", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etags
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag", tag_buffer);

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    uint32_t size;
    r = Get(cache, "uri", "etag", &buffer, &checksum, &size);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_EQ(checksum, dmHashBuffer64(buffer, size));
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    ASSERT_EQ(1U, dmHttpCache::GetEntryCount(cache));

    dmSys::IterateTree(m_Path, false, false, 0, MissingContent_Dirs);

    // Get content, ensure that the checksum is incorrect
    r = Get(cache, "uri", "etag", &buffer, &checksum, &size);
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);
    ASSERT_EQ(0U, dmHttpCache::GetEntryCount(cache));

    // Close
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, UpdateReadlocked)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri", "etag", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    FILE* file;
    uint64_t checksum;
    r = dmHttpCache::Get(cache, "uri", "etag", &file, 0, &checksum);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri", "etag", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_ALREADY_CACHED, r);

    r = Put(cache, "uri", "etag2", "data", strlen("data"));
    ASSERT_EQ(dmHttpCache::RESULT_LOCKED, r);

    dmHttpCache::Release(cache, "uri", "etag", file);

    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, GetWriteLocked)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    dmHttpCache::HCacheCreator cache_creator;
    r = dmHttpCache::Begin(cache, "uri", "etag", 0, &cache_creator);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    FILE* file;
    uint64_t checksum;
    r = dmHttpCache::Get(cache, "uri", "etag", &file, 0, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_LOCKED, r);

    dmHttpCache::Add(cache, cache_creator, "data", 4);
    dmHttpCache::End(cache, cache_creator);

    void* buffer = 0;
    r = Get(cache, "uri", "etag", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, DoublePut)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    dmHttpCache::HCacheCreator cache_creator1;
    r = dmHttpCache::Begin(cache, "uri", "etag", 0, &cache_creator1);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    dmHttpCache::HCacheCreator cache_creator2;
    r = dmHttpCache::Begin(cache, "uri", "etag2", 0, &cache_creator2);
    ASSERT_EQ(dmHttpCache::RESULT_LOCKED, r);

    dmHttpCache::Add(cache, cache_creator1, "data", 4);
    dmHttpCache::End(cache, cache_creator1);

    void* buffer = 0;
    uint64_t checksum;
    r = Get(cache, "uri", "etag", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64("data"), checksum);
    ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
    free(buffer);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, PartialUpdate)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    dmHttpCache::HCacheCreator cache_creator;
    r = dmHttpCache::Begin(cache, "uri", "etag", 0, &cache_creator);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    FILE* file;
    uint64_t checksum;
    r = dmHttpCache::Get(cache, "uri", "etag", &file, 0, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_LOCKED, r);

    dmHttpCache::Add(cache, cache_creator, "data", 4);

    // Close and reopen
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    void* buffer = 0;
    r = Get(cache, "uri", "etag", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);

    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, Stress1)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    for (int i = 0; i < 100; ++i)
    {
        char etag[16];
        dmSnPrintf(etag, sizeof(etag), "%i", i);
        r = Put(cache, "uri", etag, "data", strlen("data"));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);

        // Get etags
        char tag_buffer[16];
        r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_STREQ(etag, tag_buffer);

        // Get content
        void* buffer = 0;
        uint64_t checksum;
        r = Get(cache, "uri", etag, &buffer, &checksum);
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_EQ(dmHashString64("data"), checksum);
        ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
        free(buffer);

    }

    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, Stress2)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    for (int i = 0; i < 100; ++i)
    {
        char uri[16];
        dmSnPrintf(uri, sizeof(uri), "%i", i);
        r = Put(cache, uri, "etag", "data", strlen("data"));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);

        // Get etags
        char tag_buffer[16];
        r = dmHttpCache::GetETag(cache, uri, tag_buffer, sizeof(tag_buffer));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_STREQ("etag", tag_buffer);

        // Get content
        void* buffer = 0;
        uint64_t checksum;
        r = Get(cache, uri, "etag", &buffer, &checksum);
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_EQ(dmHashString64("data"), checksum);
        ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
        free(buffer);

    }

    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, StressOpenClose)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;

    // Open/Close cache for every iteration
    for (int i = 0; i < 100; ++i)
    {
        dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);

        char etag[16];
        dmSnPrintf(etag, sizeof(etag), "%i", i);
        r = Put(cache, "uri", etag, "data", strlen("data"));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);

        // Get etags
        char tag_buffer[16];
        r = dmHttpCache::GetETag(cache, "uri", tag_buffer, sizeof(tag_buffer));
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_STREQ(etag, tag_buffer);

        // Get content
        void* buffer = 0;
        uint64_t checksum;
        r = Get(cache, "uri", etag, &buffer, &checksum);
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
        ASSERT_EQ(dmHashString64("data"), checksum);
        ASSERT_TRUE(memcmp("data", buffer, strlen("data")) == 0);
        free(buffer);

        r = dmHttpCache::Close(cache);
        ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    }
}

TEST_F(dmHttpCacheTest, PutGet)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Put cache data
    const char* data1 = "data1";
    const char* data2 = "data2";
    const char* data3 = "data3";
    r = Put(cache, "uri1", "etag1", data1, strlen(data1));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri2", "etag2", data2, strlen(data2));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    r = Put(cache, "uri3", "etag3", data3, strlen(data3));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etags
    char tag_buffer[16];
    r = dmHttpCache::GetETag(cache, "uri1", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag1", tag_buffer);

    r = dmHttpCache::GetETag(cache, "uri2", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag2", tag_buffer);

    r = dmHttpCache::GetETag(cache, "uri3", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag3", tag_buffer);

    // Replace uri1
    const char* data1_prim = "data1_prim";
    r = Put(cache, "uri1", "etag1_prim", data1_prim, strlen(data1_prim));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);

    // Get etags again
    r = dmHttpCache::GetETag(cache, "uri1", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag1_prim", tag_buffer);

    r = dmHttpCache::GetETag(cache, "uri2", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag2", tag_buffer);

    r = dmHttpCache::GetETag(cache, "uri3", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_STREQ("etag3", tag_buffer);

    // Redundant set
    r = Put(cache, "uri1", "etag1_prim", data1_prim, strlen(data1_prim));
    ASSERT_EQ(dmHttpCache::RESULT_ALREADY_CACHED, r);

    r = Put(cache, "uri2", "etag2", data2, strlen(data2));
    ASSERT_EQ(dmHttpCache::RESULT_ALREADY_CACHED, r);

    r = Put(cache, "uri3", "etag3", data3, strlen(data3));
    ASSERT_EQ(dmHttpCache::RESULT_ALREADY_CACHED, r);

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    r = Get(cache, "uri1", "etag1_prim", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data1_prim), checksum);
    ASSERT_TRUE(memcmp(data1_prim, buffer, strlen(data1_prim)) == 0);
    free(buffer);

    r = Get(cache, "uri2", "etag2", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data2), checksum);
    ASSERT_TRUE(memcmp(data2, buffer, strlen(data2)) == 0);
    free(buffer);

    r = Get(cache, "uri3", "etag3", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data3), checksum);
    ASSERT_TRUE(memcmp(data3, buffer, strlen(data3)) == 0);
    free(buffer);

    // No entry test
    r = dmHttpCache::GetETag(cache, "no_entry_uri", tag_buffer, sizeof(tag_buffer));
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);

    r = Get(cache, "no_entry_uri", "etag3", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);

    r = Get(cache, "uri1", "no_entry_tag", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, r);

    // Free
    r = dmHttpCache::Close(cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
}

TEST_F(dmHttpCacheTest, Persist)
{
    dmHttpCache::HCache cache;
    dmHttpCache::NewParams params;
    params.m_Path = m_Path;
    dmHttpCache::Result r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(0U, dmHttpCache::GetEntryCount(cache));

    // Put cache data
    const char* data1 = "data1";
    const char* data2 = "data2";
    const char* data3 = "data3";
    r = Put(cache, "uri1", "etag1", data1, strlen(data1));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = Put(cache, "uri2", "etag2", data2, strlen(data2));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    r = Put(cache, "uri3", "etag3", data3, strlen(data3));
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(3U, dmHttpCache::GetEntryCount(cache));

    dmHttpCache::Close(cache);
    r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(3U, dmHttpCache::GetEntryCount(cache));

    // Get content
    void* buffer = 0;
    uint64_t checksum;
    r = Get(cache, "uri1", "etag1", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data1), checksum);
    ASSERT_TRUE(memcmp(data1, buffer, strlen(data1)) == 0);
    free(buffer);

    r = Get(cache, "uri2", "etag2", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data2), checksum);
    ASSERT_TRUE(memcmp(data2, buffer, strlen(data2)) == 0);
    free(buffer);

    r = Get(cache, "uri3", "etag3", &buffer, &checksum);
    ASSERT_EQ(dmHttpCache::RESULT_OK, r);
    ASSERT_EQ(dmHashString64(data3), checksum);
    ASSERT_TRUE(memcmp(data3, buffer, strlen(data3)) == 0);
    free(buffer);

    dmHttpCache::Close(cache);
    // Set max age to 1 seconds and sleep for 1.5 seconds
    params.m_MaxCacheEntryAge = 1;
    dmTime::Sleep((1000000 * 3) / 2);
    r = dmHttpCache::Open(&params, &cache);
    ASSERT_EQ(0U, dmHttpCache::GetEntryCount(cache));
    dmHttpCache::Close(cache);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
