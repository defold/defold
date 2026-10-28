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

#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/http/http_cache.h>
#include <dlib/http/http_client.h>
#include <dlib/message.h>
#include <dlib/socket.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>
#include <ddf/ddf.h>
#include <resource/resource_ddf.h>

#include "../providers/provider.h"
#include "../providers/provider_private.h"
#include "../resource.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static int32_t PORT = 6123; // can be configured

// File generated with
// data = b"\x00\x01\x02\x03\x04\x05\x06\x07"
// with open('./src/test/files/src/test/files/somedata', 'wb') as f:
//     f.write(data)
const uint8_t SOMEDATA[] = {0, 1, 2, 3, 4, 5, 6, 7};

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

TEST(HttpProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_EQ((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(HttpProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    dmURI::Parts uri;
    dmURI::Parse(".", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("file:some/folder", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("dmanif:some/folder", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("http://domain.com/path", &uri);
    ASSERT_TRUE(loader->m_CanMount(&uri));
}


class HttpProviderArchive : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        char buffer[128];
        dmSnPrintf(buffer, sizeof(buffer), "http://localhost:%d", PORT);

        dmURI::Parts uri;
        dmURI::Parse(buffer, &uri);

        dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    void TearDown() override
    {
        dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmhash_t m_NameHashArchive;
    dmhash_t m_NameHashFile;

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::ArchiveLoader* m_Loader;
};


TEST_F(HttpProviderArchive, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/test.cont", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(35U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/test_ref.cont", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(25U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/not_exist", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

TEST_F(HttpProviderArchive, ReadFile)
{
    dmResourceProvider::Result result;
    uint8_t short_buffer[4] = {0};
    uint8_t long_buffer[64] = {0};

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", short_buffer, sizeof(short_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, result);

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", long_buffer, sizeof(long_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, long_buffer, sizeof(SOMEDATA));
}

#if !defined(DM_TEST_NO_SUPPORT_RANGE_REQUEST)

TEST_F(HttpProviderArchive, ReadFilePartial)
{
    dmResourceProvider::Result result;
    uint8_t long_buffer[64];

    uint32_t file_size;
    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/somedata", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", long_buffer, sizeof(long_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, long_buffer, sizeof(SOMEDATA));

    const uint32_t chunk_size = 5;
    uint8_t short_buffer[chunk_size];

    uint32_t offset = 0;
    while (offset < file_size)
    {
        uint32_t nread;
        result = dmResourceProvider::ReadFilePartial(m_Archive, 0, "/somedata", offset, chunk_size, short_buffer, &nread);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        ASSERT_GE(chunk_size, nread);

        ASSERT_ARRAY_EQ_LEN(&long_buffer[offset], short_buffer, nread);
        offset += nread;
    }
}
#endif // DM_TEST_NO_SUPPORT_RANGE_REQUEST

// Unknown-length and chunked bodies must fit the caller's destination, and a
// server ignoring Range must still return the requested interval rather than byte zero.
TEST_F(HttpProviderArchive, StreamingDestinations)
{
    const char* paths[] = { "/large", "/chunked", "/unknown" };
    const uint32_t size = 128 * 1024;
    uint8_t* buffer = new uint8_t[size + 2];
    for (uint32_t p = 0; p < sizeof(paths) / sizeof(paths[0]); ++p)
    {
        memset(buffer, 0xfe, size + 2);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, paths[p], buffer + 1, size));
        for (uint32_t i = 0; i < size; ++i)
            ASSERT_EQ((uint8_t)(i % 251), buffer[i + 1]);
        ASSERT_EQ(0xfe, buffer[0]);
        ASSERT_EQ(0xfe, buffer[size + 1]);

        memset(buffer, 0xfe, size + 2);
        ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, dmResourceProvider::ReadFile(m_Archive, 0, paths[p], buffer + 1, 4));
        ASSERT_EQ(0xfe, buffer[0]);
        ASSERT_EQ(0xfe, buffer[5]);

        uint32_t nread = UINT32_MAX; // Output only, never the destination capacity.
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, paths[p], size - 7, 16, buffer + 1, &nread));
        ASSERT_EQ(7U, nread);
        for (uint32_t i = 0; i < nread; ++i)
            ASSERT_EQ((uint8_t)((size - 7 + i) % 251), buffer[i + 1]);
        ASSERT_EQ(0xfe, buffer[0]);
        ASSERT_EQ(0xfe, buffer[17]);
    }
    delete[] buffer;
}

// Metadata-only reads and failed/empty ranges must not allocate a response or
// corrupt destination guards; a new response must reset errors from a retry.
TEST_F(HttpProviderArchive, MetadataErrorsAndRetry)
{
    uint32_t size;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/huge-head", &size));
    ASSERT_EQ(1024U * 1024 * 1024, size);
    uint8_t buffer[10];
    memset(buffer, 0xfe, sizeof(buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, "/retry", buffer + 1, 8));
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, buffer + 1, 8);
    const char* errors[] = { "/not_exist", "/bad-range", "/truncated" };
    for (uint32_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i)
    {
        uint32_t nread = 123;
        ASSERT_NE(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, errors[i], 3, 5, buffer + 1, &nread));
        ASSERT_EQ(0U, nread);
    }
    uint32_t nread = 123;
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/somedata", UINT32_MAX, 2, buffer + 1, &nread));
    ASSERT_EQ(0U, nread);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/somedata", 0, 0, buffer + 1, &nread));
    ASSERT_EQ(0U, nread);
    ASSERT_EQ(0xfe, buffer[0]);
    ASSERT_EQ(0xfe, buffer[9]);
}

class HttpProviderCached : public HttpProviderArchive
{
protected:
    dmHttpCache::HCache m_Cache;
    void SetUp() override
    {
        dmHttpCache::NewParams cache_params;
        cache_params.m_Path = "cache";
        ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Open(&cache_params, &m_Cache));
        dmResourceProvider::ArchiveLoaderParams params = {};
        params.m_HttpCache = m_Cache;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::InitializeLoaders(&params));
        HttpProviderArchive::SetUp();
    }
    void TearDown() override
    {
        HttpProviderArchive::TearDown();
        dmResourceProvider::ArchiveLoaderParams params = {};
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::FinalizeLoaders(&params));
        ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Close(m_Cache));
    }
};

// Exercise network, conditional 304 and trusted cache replay for both complete
// files and ranges, including HEAD after a cached partial read.
TEST_F(HttpProviderCached, Replay)
{
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 2)
            dmHttpCache::SetConsistencyPolicy(m_Cache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
        uint8_t buffer[8];
        memset(buffer, 0xfe, sizeof(buffer));
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", buffer, sizeof(buffer)));
        ASSERT_ARRAY_EQ_LEN(SOMEDATA, buffer, sizeof(buffer));
        uint32_t nread = UINT32_MAX;
        memset(buffer, 0xfe, sizeof(buffer));
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/somedata", 5, 5, buffer + 1, &nread));
        ASSERT_EQ(3U, nread);
        ASSERT_ARRAY_EQ_LEN(SOMEDATA + 5, buffer + 1, nread);
        ASSERT_EQ(0xfe, buffer[0]);
        ASSERT_EQ(0xfe, buffer[4]);
        uint32_t size;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/somedata", &size));
        ASSERT_EQ(8U, size);
    }
}

// Ranges with an unknown total size must retain their offsets and metadata
// through network reads, conditional 304 replay and trusted cache replay.
TEST_F(HttpProviderCached, UnknownRangeSize)
{
    const char* paths[] = { "/unknown-range", "/unknown-range-chunked" };
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 2)
            dmHttpCache::SetConsistencyPolicy(m_Cache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
        for (uint32_t p = 0; p < sizeof(paths) / sizeof(paths[0]); ++p)
        {
            uint8_t buffer[7];
            memset(buffer, 0xfe, sizeof(buffer));
            uint32_t nread = UINT32_MAX;
            ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, paths[p], 5, 5, buffer + 1, &nread));
            ASSERT_EQ(3U, nread);
            ASSERT_ARRAY_EQ_LEN(SOMEDATA + 5, buffer + 1, nread);
            ASSERT_EQ(0xfe, buffer[0]);
            ASSERT_EQ(0xfe, buffer[4]);
            ASSERT_EQ(0xfe, buffer[6]);

            char cache_key[128];
            dmSnPrintf(cache_key, sizeof(cache_key), "http://localhost:%d%s=bytes=5-9", PORT, paths[p]);
            dmHttpCache::EntryInfo info;
            ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::GetInfo(m_Cache, cache_key, &info));
            ASSERT_EQ(5U, info.m_RangeStart);
            ASSERT_EQ(7U, info.m_RangeEnd);
            ASSERT_EQ(UINT32_MAX, info.m_DocumentSize);
            if (pass > 0)
                ASSERT_TRUE(info.m_Verified);
        }
    }
}

// Invalid range bodies must never enter the cache, where a later 304 or trusted
// replay could otherwise bypass validation of the original 206 response.
TEST_F(HttpProviderCached, RejectInvalidRangeBeforeCaching)
{
    const char* paths[] = { "/cached-bad-range", "/cached-bad-range-chunked", "/cached-bad-range-unknown" };
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 2)
            dmHttpCache::SetConsistencyPolicy(m_Cache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
        for (uint32_t p = 0; p < sizeof(paths) / sizeof(paths[0]); ++p)
        {
            uint8_t buffer[7];
            memset(buffer, 0xfe, sizeof(buffer));
            uint32_t nread = UINT32_MAX;
            ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, dmResourceProvider::ReadFilePartial(m_Archive, 0, paths[p], 3, 5, buffer + 1, &nread));
            ASSERT_EQ(0U, nread);
            ASSERT_EQ(0xfe, buffer[0]);
            ASSERT_EQ(0xfe, buffer[6]);

            char cache_key[128];
            dmSnPrintf(cache_key, sizeof(cache_key), "http://localhost:%d%s=bytes=3-7", PORT, paths[p]);
            dmHttpCache::EntryInfo info;
            ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, dmHttpCache::GetInfo(m_Cache, cache_key, &info));
        }
    }
}

class HttpProviderBatch : public HttpProviderCached
{
protected:
    char m_MountPath[64];

    static void ControlContent(dmHttpClient::HResponse, void* user_data, int, const void* data, uint32_t size,
                               int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        uint32_t* value = (uint32_t*)user_data;
        for (uint32_t i = 0; i < size; ++i)
            *value = *value * 10 + ((const char*)data)[i] - '0';
    }

    void Control(const char* command, uint32_t expected)
    {
        char url[128];
        dmSnPrintf(url, sizeof(url), "http://localhost:%d", PORT);
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        uint32_t value = 0;
        dmHttpClient::NewParams params;
        params.m_Userdata = &value;
        params.m_HttpContent = ControlContent;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        ASSERT_NE((void*)0, client);
        char path[128];
        dmSnPrintf(path, sizeof(path), "/__test/%s", command);
        dmHttpClient::Result result = dmHttpClient::Get(client, path);
        dmHttpClient::Delete(client);
        ASSERT_EQ(dmHttpClient::RESULT_OK, result);
        ASSERT_EQ(expected, value);
    }

    void Remount()
    {
        char url[128];
        dmSnPrintf(url, sizeof(url), "http://localhost:%d%s", PORT, m_MountPath);
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmResourceProvider::HArchive archive;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(m_Loader, &uri, 0, &archive));
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::Unmount(m_Archive));
        m_Archive = archive;
    }

    void SetUp() override
    {
        Control("reset", 0);
        HttpProviderCached::SetUp();
        static uint32_t mount_id = 0;
        dmSnPrintf(m_MountPath, sizeof(m_MountPath), "/build/batch-%u", ++mount_id);
        Remount();
    }

    void Read(const char* path, uint32_t expected_size)
    {
        uint32_t size = 0;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, path, &size));
        ASSERT_EQ(expected_size, size);
        uint8_t data[16];
        memset(data, 0xff, sizeof(data));
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, path, data, size));
        for (uint32_t i = 0; i < expected_size; ++i)
            ASSERT_EQ(i, data[i]);
        ASSERT_EQ(0xff, data[expected_size]);
    }
};

// One batch must validate only this mount, without changing the shared cache
// policy, and eliminate both HEAD and GET for unchanged cached resources.
TEST_F(HttpProviderBatch, WarmMount)
{
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Control("counts-reset", 0);
    Remount();
    Control("verifications", 1);
    Control("entries", 2);
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Control("requests", 0);
    ASSERT_EQ(dmHttpCache::CONSISTENCY_POLICY_VERIFY, dmHttpCache::GetConsistencyPolicy(m_Cache));
}

// A large verification snapshot must preserve every key and body across
// multiple growth boundaries in the entry array, lookup table and request.
TEST_F(HttpProviderBatch, LargeVerificationSnapshot)
{
    const uint32_t count = 1025;
    for (uint32_t i = 0; i < count; ++i)
    {
        char key[256];
        dmSnPrintf(key, sizeof(key), "http://localhost:%d%s/entry-%u.bin", PORT, m_MountPath, i);
        dmHttpCache::HCacheCreator creator;
        ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Begin(m_Cache, key, "\"version-1\"", 0, 0, 7, 8, &creator));
        ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Add(m_Cache, creator, SOMEDATA, 8));
        ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::End(m_Cache, creator));
    }
    Control("counts-reset", 0);
    Remount();
    Control("verifications", 1);
    Control("entries", count);
    for (uint32_t i = 0; i < count; ++i)
    {
        char path[64];
        dmSnPrintf(path, sizeof(path), "/entry-%u.bin", i);
        Read(path, 8);
    }
    Control("requests", 0);
}

// A new build must revalidate even with the same live cache, downloading only
// the changed file and using its new size instead of old cached metadata.
TEST_F(HttpProviderBatch, ChangedBuild)
{
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Control("change", 0);
    Control("counts-reset", 0);
    Remount();
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 12);
    Control("verifications", 1);
    Control("requests", 2);
}

// Hot reload invalidates the requested file's mount-local validation while
// unrelated resources remain available directly from the verified cache.
TEST_F(HttpProviderBatch, InvalidateReloadedFile)
{
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Remount();
    Control("change", 0);
    Control("counts-reset", 0);
    dmResourceProvider::InvalidateFileCache(m_Archive, dmHashString64("/changed.bin"), "/changed.bin");
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 12);
    Control("requests", 2);
}

// An unavailable, malformed, or incomplete batch response must never grant
// cache trust, including records parsed before a later response failure.
TEST_F(HttpProviderBatch, VerificationFallback)
{
    Read("/unchanged.bin", 8);
    const char* modes[] = { "unavailable", "invalid", "truncated" };
    for (uint32_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
    {
        Control(modes[i], 0);
        Control("counts-reset", 0);
        Remount();
        Read("/unchanged.bin", 8);
        // A truncated response may retry depending on connection reuse.
        if (i != 2)
            Control("verifications", 1);
        Control("requests", 2);
    }
}

// Preserve long and escaped cache keys through fragmented records with no
// final newline in the editor's response.
TEST_F(HttpProviderBatch, VerificationCacheKeys)
{
    char long_path[700];
    memset(long_path, 'a', sizeof(long_path));
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = '\0';
    Read(long_path, 8);
    Read("/space file.bin", 8);
    Control("counts-reset", 0);
    Remount();
    Read(long_path, 8);
    Read("/space file.bin", 8);
    Control("entries", 2);
    Control("requests", 0);
}

// Exact URL keys must not confuse a literal percent escape in one
// filename with a different, changed resource containing a space.
TEST_F(HttpProviderBatch, EscapedCacheKeys)
{
    Read("/space file.bin", 8);
    Read("/space%20file.bin", 8);
    Control("change", 0);
    Control("counts-reset", 0);
    Remount();
    Read("/space%20file.bin", 8);
    Control("requests", 0);
    Read("/space file.bin", 12);
    Control("requests", 2);
}

// ReloadResource must invalidate through the factory and mount layers even
// for raw resources, which have no loaded resource descriptor to recreate.
TEST_F(HttpProviderBatch, ReloadRawResource)
{
    Read("/changed.bin", 8);
    char url[128];
    dmSnPrintf(url, sizeof(url), "http://localhost:%d%s", PORT, m_MountPath);
    dmResource::NewFactoryParams params;
    params.m_HttpCache = m_Cache;
    dmResource::HFactory factory = dmResource::NewFactory(&params, url);
    ASSERT_NE((void*)0, factory);

    Control("counts-reset", 0);
    void* data;
    uint32_t size;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(factory, "/changed.bin", &data, &size));
    ASSERT_EQ(8U, size);
    free(data);
    Control("requests", 0);

    Control("change", 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::ReloadResource(factory, "/changed.bin", 0));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(factory, "/changed.bin", &data, &size));
    ASSERT_EQ(12U, size);
    ASSERT_EQ(11U, ((uint8_t*)data)[11]);
    free(data);
    Control("requests", 2);
    dmResource::DeleteFactory(factory);
}

struct ReloadBatchContext
{
    dmResource::HFactory m_Factory;
    dmResource::Result   m_ReadResult;
    uint32_t            m_CallbackCount;
    uint32_t            m_Size;
    uint8_t             m_LastByte;
};

static dmResource::Result ReloadBatchCreate(const dmResource::ResourceCreateParams* params)
{
    ResourceDescriptorSetResource(params->m_Resource, params->m_Context);
    return dmResource::RESULT_OK;
}

static dmResource::Result ReloadBatchDestroy(const dmResource::ResourceDestroyParams*)
{
    return dmResource::RESULT_OK;
}

static dmResource::Result ReloadBatchRecreate(const dmResource::ResourceRecreateParams*)
{
    return dmResource::RESULT_OK;
}

static void ReloadBatchCallback(const dmResource::ResourceReloadedParams* params)
{
    ReloadBatchContext* context = (ReloadBatchContext*)params->m_UserData;
    ++context->m_CallbackCount;
    void* data = 0;
    context->m_ReadResult = dmResource::GetRaw(context->m_Factory, "/changed.bin", &data, &context->m_Size);
    if (context->m_ReadResult == dmResource::RESULT_OK && context->m_Size)
        context->m_LastByte = ((uint8_t*)data)[context->m_Size - 1];
    free(data);
}

// A reload callback must see changed raw resources later in the same message,
// including string offsets above 255, while unrelated resources remain cached.
TEST_F(HttpProviderBatch, InvalidateBatchBeforeReloadCallbacks)
{
    char driver_path[256];
    memset(driver_path, 'a', sizeof(driver_path));
    driver_path[0] = '/';
    dmStrlCpy(driver_path + sizeof(driver_path) - sizeof(".foo"), ".foo", sizeof(".foo"));
    Read(driver_path, 8);
    Read("/changed.bin", 8);
    Read("/unchanged.bin", 8);
    char url[128];
    dmSnPrintf(url, sizeof(url), "http://localhost:%d%s", PORT, m_MountPath);
    dmResource::NewFactoryParams params;
    params.m_HttpCache = m_Cache;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, url);
    ASSERT_NE((void*)0, factory);

    ReloadBatchContext context = {};
    context.m_Factory = factory;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "foo", &context, 0, ReloadBatchCreate, 0, ReloadBatchDestroy, ReloadBatchRecreate));
    dmResource::RegisterResourceReloadedCallback(factory, ReloadBatchCallback, &context);
    void* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(factory, driver_path, &resource));

    Control("change", 0);
    Control("counts-reset", 0);
    const char* paths[] = { driver_path, "/changed.bin" };
    dmResourceDDF::Reload reload = {};
    reload.m_Resources.m_Data = paths;
    reload.m_Resources.m_Count = sizeof(paths) / sizeof(paths[0]);
    dmArray<uint8_t> encoded;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&reload, dmResourceDDF::Reload::m_DDFDescriptor, encoded));
    void* message;
    uint32_t message_size;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(encoded.Begin(), encoded.Size(), dmResourceDDF::Reload::m_DDFDescriptor, &message,
                                                dmDDF::OPTION_OFFSET_POINTERS, &message_size));
    dmMessage::URL receiver = {};
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::GetSocket("@resource", &receiver.m_Socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0, &receiver, dmResourceDDF::Reload::m_DDFHash, 0,
                                                 (uintptr_t)dmResourceDDF::Reload::m_DDFDescriptor, message, message_size, 0));
    dmDDF::FreeMessage(message);
    dmResource::UpdateFactory(factory);

    void* data;
    uint32_t size;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(factory, "/unchanged.bin", &data, &size));
    ASSERT_EQ(8U, size);
    free(data);
    dmResource::UnregisterResourceReloadedCallback(factory, ReloadBatchCallback, &context);
    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);

    ASSERT_EQ(1U, context.m_CallbackCount);
    ASSERT_EQ(dmResource::RESULT_OK, context.m_ReadResult);
    ASSERT_EQ(12U, context.m_Size);
    ASSERT_EQ(11U, context.m_LastByte);
    Control("requests", 4);
}

// Replacing an entry after validation must not transfer trust to its new ETag.
TEST_F(HttpProviderBatch, ReplacedCacheEntry)
{
    Read("/unchanged.bin", 8);
    Remount();
    char key[256];
    dmSnPrintf(key, sizeof(key), "http://localhost:%d%s/unchanged.bin", PORT, m_MountPath);
    dmHttpCache::HCacheCreator creator;
    ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Begin(m_Cache, key, "unverified-tag", 0, 0, 0, 1, &creator));
    ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::Add(m_Cache, creator, "x", 1));
    ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::End(m_Cache, creator));
    Control("counts-reset", 0);
    Read("/unchanged.bin", 8);
    Control("requests", 2);
}

class HttpProviderIndex : public HttpProviderBatch
{
protected:
    void SetUp() override
    {
        HttpProviderBatch::SetUp();
        char command[128];
        dmSnPrintf(command, sizeof(command), "index%s", m_MountPath);
        Control(command, 0);
        Remount();
        Control("counts-reset", 0);
    }
};

// The index removes cold HEADs, validates warm bodies in the same request,
// and supplies updated sizes when one resource changes between builds.
TEST_F(HttpProviderIndex, ColdWarmAndChanged)
{
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Read("/empty.bin", 0);
    uint32_t size;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/huge.bin", &size));
    ASSERT_EQ(1024U * 1024 * 1024, size);
    Control("requests", 3);

    Control("counts-reset", 0);
    Remount();
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Control("indices", 1);
    Control("verifications", 0);
    Control("requests", 0);

    Control("change", 0);
    Remount();
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 12);
    Control("requests", 1);
    ASSERT_EQ(dmHttpCache::CONSISTENCY_POLICY_VERIFY, dmHttpCache::GetConsistencyPolicy(m_Cache));
}

// Encoded names remain distinct and entries from another mount cannot supply
// sizes; resources absent from the index still use ordinary HTTP requests.
TEST_F(HttpProviderIndex, EscapedNamesAndMountScope)
{
    Read("/space file.bin", 8);
    Read("/space%20file.bin", 8);
    Read("/a+b.bin", 8);
    Read("/outside.bin", 8);
    Control("requests", 5);
    Control("change", 0);
    Control("counts-reset", 0);
    Remount();
    Read("/space%20file.bin", 8);
    Read("/a+b.bin", 8);
    Read("/space file.bin", 12);
    Control("requests", 1);
}

// Invalid or interrupted indices must discard all provisional sizes and ETag
// matches, even if valid records arrived before the failure.
TEST_F(HttpProviderIndex, InvalidIndexFallback)
{
    Read("/unchanged.bin", 8);
    Control("unavailable", 0);
    const char* modes[] = { "index-unavailable", "index-oversized", "index-invalid-size", "index-invalid-path",
                            "index-duplicate", "index-incomplete", "index-version", "index-truncated" };
    for (uint32_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
    {
        Control(modes[i], 0);
        Control("counts-reset", 0);
        Remount();
        Read("/unchanged.bin", 8);
        Control("requests", 2);
    }
}

// Hot reload invalidates both the indexed size and cache trust for the changed
// resource, while unrelated resources remain available without network I/O.
TEST_F(HttpProviderIndex, InvalidateIndexedResource)
{
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Remount();
    Control("change", 0);
    Control("counts-reset", 0);
    dmResourceProvider::InvalidateFileCache(m_Archive, dmHashString64("/changed.bin"), "/changed.bin");
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 12);
    Control("requests", 2);
}

// A build changing during a mount must fail a read with a stale indexed size,
// including shrinking files, and discard that size so a retry can recover.
TEST_F(HttpProviderIndex, StaleIndexSize)
{
    Control("change", 0);
    uint8_t data[16];
    memset(data, 0xfe, sizeof(data));
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, dmResourceProvider::ReadFile(m_Archive, 0, "/changed.bin", data, 8));
    ASSERT_EQ(0xfe, data[8]);
    Read("/changed.bin", 12);

    memset(data, 0xfe, sizeof(data));
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, dmResourceProvider::ReadFile(m_Archive, 0, "/shrink.bin", data, 12));
    ASSERT_EQ(0xfe, data[8]);
    Read("/shrink.bin", 8);
}

// Index metadata is useful even when the factory has no HTTP body cache.
TEST_F(HttpProviderIndex, WithoutBodyCache)
{
    dmResourceProvider::ArchiveLoaderParams params = {};
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::InitializeLoaders(&params));
    Remount();
    Control("counts-reset", 0);
    Read("/unchanged.bin", 8);
    Control("requests", 1);
}

// Full-file index metadata must not turn a range read into full-file cache
// replay; partial reads keep their offsets and destination bounds.
TEST_F(HttpProviderIndex, PartialRead)
{
    Read("/unchanged.bin", 8);
    Remount();
    Control("counts-reset", 0);
    uint32_t size;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, "/unchanged.bin", &size));
    ASSERT_EQ(8U, size);
    uint8_t data[7];
    memset(data, 0xfe, sizeof(data));
    uint32_t nread;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/unchanged.bin", 3, 5, data + 1, &nread));
    ASSERT_EQ(5U, nread);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA + 3, data + 1, 5);
    ASSERT_EQ(0xfe, data[0]);
    ASSERT_EQ(0xfe, data[6]);
    Control("requests", 1);
}

class HttpProviderDownloads : public HttpProviderIndex
{
protected:
    void SetUp() override
    {
        HttpProviderIndex::SetUp();
        Control("download-normal", 0);
    }

    void ReadSized(const char* path, uint32_t expected_size)
    {
        uint32_t size;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::GetFileSize(m_Archive, 0, path, &size));
        ASSERT_EQ(expected_size, size);
        uint8_t* data = new uint8_t[size + 2];
        memset(data, 0xfe, size + 2);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFile(m_Archive, 0, path, data + 1, size));
        for (uint32_t i = 0; i < size; ++i)
            ASSERT_EQ((uint8_t)(i % 251), data[i + 1]);
        ASSERT_EQ(0xfe, data[0]);
        ASSERT_EQ(0xfe, data[size + 1]);
        delete[] data;
    }
};

// A deleted resource that was only prefetched must become unavailable after
// hot reload, while other prefetched resources remain trusted.
TEST_F(HttpProviderDownloads, DeletedAfterPrefetch)
{
    char url[128];
    dmSnPrintf(url, sizeof(url), "http://localhost:%d%s", PORT, m_MountPath);
    dmResource::NewFactoryParams params;
    params.m_HttpCache = m_Cache;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, url);
    ASSERT_NE((void*)0, factory);

    void* data;
    uint32_t size;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(factory, "/unchanged.bin", &data, &size));
    free(data);
    Control("downloads", 1);
    Control("requests", 0);
    char cache_key[256];
    dmSnPrintf(cache_key, sizeof(cache_key), "%s/changed.bin", url);
    dmHttpCache::EntryInfo info;
    ASSERT_EQ(dmHttpCache::RESULT_OK, dmHttpCache::GetInfo(m_Cache, cache_key, &info));

    Control("delete", 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::ReloadResource(factory, "/changed.bin", 0));
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::GetRaw(factory, "/changed.bin", &data, &size));
    Control("requests", 1);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(factory, "/space file.bin", &data, &size));
    ASSERT_EQ(8U, size);
    free(data);
    Control("requests", 1);
    dmResource::DeleteFactory(factory);
}

// One streamed batch populates separate cache entries, including empty files
// and escaped names; later mounts transfer only resources with changed ETags.
TEST_F(HttpProviderDownloads, ColdWarmAndChanged)
{
    Read("/changed.bin", 8);
    Read("/unchanged.bin", 8);
    Read("/empty.bin", 0);
    Read("/space file.bin", 8);
    Read("/space%20file.bin", 8);
    Read("/a+b.bin", 8);
    Read("/shrink.bin", 12);
    Control("downloads", 1);
    Control("download-bytes", 52);
    Control("download-sorted", 1);
    Control("requests", 0);

    Remount();
    Control("counts-reset", 0);
    Read("/unchanged.bin", 8);
    Read("/empty.bin", 0);
    Control("downloads", 0);
    Control("requests", 0);

    Control("change", 0);
    Remount();
    Read("/changed.bin", 12);
    Read("/space file.bin", 12);
    Read("/shrink.bin", 8);
    Control("downloads", 1);
    Control("download-bytes", 32);
    Control("requests", 0);
}

// Exactly 1 MiB files are eligible, larger files remain individual, and the
// requested file leads size-sorted batches containing at most 4 MiB of data.
TEST_F(HttpProviderDownloads, SizeLimitsAndPriority)
{
    Control("download-large", 0);
    Remount();
    Control("counts-reset", 0);
    ReadSized("/limit-7.bin", 1024 * 1024);
    Control("download-first-size", 1024 * 1024);
    Control("download-sorted", 1);
    for (uint32_t i = 0; i < 7; ++i)
    {
        char path[64];
        dmSnPrintf(path, sizeof(path), "/limit-%u.bin", i);
        ReadSized(path, 1024 * 1024);
    }
    Control("downloads", 3);
    Control("download-max-bytes", 4 * 1024 * 1024);
    Control("download-bytes", 8 * 1024 * 1024 + 52);
    ReadSized("/over-limit.bin", 1024 * 1024 + 1);
    Control("downloads", 3);
    Control("requests", 1);
}

// Unsupported, stale, malformed and interrupted batches fall back once per
// mount, with no incomplete resource left available as a trusted cache entry.
TEST_F(HttpProviderDownloads, Fallback)
{
    char base[64];
    dmStrlCpy(base, m_MountPath, sizeof(base));
    const char* modes[] = { "unavailable", "stale", "invalid-tag", "invalid-size", "invalid-path", "truncated-first" };
    for (uint32_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
    {
        dmSnPrintf(m_MountPath, sizeof(m_MountPath), "%s-fallback-%u", base, i);
        char command[128];
        dmSnPrintf(command, sizeof(command), "index%s", m_MountPath);
        Control(command, 0);
        dmSnPrintf(command, sizeof(command), "download-%s", modes[i]);
        Control(command, 0);
        Remount();
        Control("counts-reset", 0);
        Read("/unchanged.bin", 8);
        Control("download-called", 1);
        Control("requests", 1);
        Control("counts-reset", 0);
        Read("/changed.bin", 8);
        Control("downloads", 0);
        Control("requests", 1);
    }
}

// Completed resources survive an interruption later in the batch; the partial
// final resource is removed and can be fetched successfully on its next read.
TEST_F(HttpProviderDownloads, KeepCompletedEntries)
{
    Control("download-truncated-last", 0);
    Read("/unchanged.bin", 8);
    Read("/changed.bin", 8);
    Control("requests", 0);
    char key[256];
    dmSnPrintf(key, sizeof(key), "http://localhost:%d%s/shrink.bin", PORT, m_MountPath);
    dmHttpCache::EntryInfo info;
    ASSERT_EQ(dmHttpCache::RESULT_NO_ENTRY, dmHttpCache::GetInfo(m_Cache, key, &info));
    Control("download-called", 1);
    Control("counts-reset", 0);
    Read("/shrink.bin", 12);
    Control("downloads", 0);
    Control("requests", 1);
}

// Hot reload must discard trust and size metadata for prefetched resources,
// while unrelated entries from the same batch remain locally readable.
TEST_F(HttpProviderDownloads, InvalidatePrefetchedResource)
{
    Read("/unchanged.bin", 8);
    Control("change", 0);
    Control("counts-reset", 0);
    dmResourceProvider::InvalidateFileCache(m_Archive, dmHashString64("/changed.bin"), "/changed.bin");
    Read("/changed.bin", 12);
    Read("/unchanged.bin", 8);
    Control("downloads", 0);
    Control("requests", 2);
}

// Range reads bypass full-file prefetch even when the resource fits a batch.
TEST_F(HttpProviderDownloads, PartialRead)
{
    uint8_t data[7];
    memset(data, 0xfe, sizeof(data));
    uint32_t nread;
    ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::ReadFilePartial(m_Archive, 0, "/changed.bin", 3, 5, data + 1, &nread));
    ASSERT_EQ(5U, nread);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA + 3, data + 1, 5);
    ASSERT_EQ(0xfe, data[0]);
    ASSERT_EQ(0xfe, data[6]);
    Control("downloads", 0);
    Control("requests", 1);
}


#if defined(DM_TEST_HTTP_SUPPORTED)

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmHashEnableReverseHash(true);

    dmSocket::Initialize();
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    if(argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", path);
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &PORT, 0, 0);
        dmConfigFile::Delete(config);
    }

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    dmSocket::Finalize();
    return result;
}

#else

int main(int argc, char **argv)
{
    return 0;
}

#endif
