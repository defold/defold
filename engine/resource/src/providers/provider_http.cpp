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

#include "provider.h"
#include "provider_private.h"
#include "../resource_util.h"

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include <dlib/http/http_client.h>
#include <dlib/http/http_cache.h>
#include <dlib/http_cache_verify.h>

#include <ctype.h>
#include <stdio.h> // debug printf
#include <stdlib.h>

namespace dmResourceProviderHttp
{

static dmHttpCache::HCache g_HttpCache = 0;

// These constants must stay synchronized, in order, with max-batch-file-size,
// max-batch-size, max-batch-files and max-batch-request-size in
// editor/src/clj/editor/hot_reload.clj. The file and payload limits must also
// match MAX_BATCH_FILE_SIZE and MAX_BATCH_SIZE in
// engine/resource/src/test/test_provider_http.py.
static const uint32_t MAX_BATCH_FILE_SIZE = 1024U * 1024U;
static const uint32_t MAX_BATCH_SIZE = 4U * 1024U * 1024U;
static const uint32_t MAX_BATCH_FILES = 4096;
static const uint32_t MAX_BATCH_REQUEST_SIZE = 1024U * 1024U;

struct IndexedResource
{
    char*    m_Path;
    char*    m_Key;
    char     m_ETag[dmHttpCache::MAX_TAG_LEN];
    dmhash_t m_KeyHash;
    uint32_t m_Size;
};

struct HttpRead
{
    uint8_t* m_Buffer;
    uint32_t m_Capacity;
    uint32_t m_Offset;
    bool     m_Partial;
    bool     m_Head;

    uint64_t m_Received;
    uint32_t m_Copied;
    int32_t  m_ContentLength;
    int      m_Status;
    bool     m_Error;
};

struct HttpProviderContext
{
    dmURI::Parts          m_BaseUri;
    dmHttpClient::HClient m_HttpClient;
    dmHttpCache::HCache   m_HttpCache;
    HttpRead*            m_Read; // Only set during a synchronous request.
    dmHashTable64<uint64_t> m_VerifiedEntries; // Cache key -> validated URI/ETag identity, owned by this mount.
    dmHashTable64<uint32_t> m_ResourceSizes; // Complete-file sizes from this mount's build snapshot.
    dmArray<IndexedResource> m_ResourceIndex; // Sorted by size for bounded, on-demand prefetch.
    bool m_BatchSupported;
};

static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                        uint32_t range_start, uint32_t range_end, uint32_t document_size,
                        const char* method)
{
    HttpRead* read = ((HttpProviderContext*)user_data)->m_Read;
    if (!content_data && !content_data_size && !method)
    {
        // A retry or cache replay starts a new body. Cached HEAD metadata and
        // non-null, zero-length EOF callbacks are not response-start signals.
        read->m_Received = 0;
        read->m_Copied = 0;
        read->m_ContentLength = -1;
        read->m_Error = false;
        read->m_Status = status_code;
        return;
    }

    read->m_Status = status_code;
    read->m_ContentLength = content_length;
    if (read->m_Head || read->m_Error || !content_data_size)
        return;

    uint64_t origin = 0;
    if (status_code == 206 || status_code == 304)
    {
        // Cached ranges retain their original byte offsets. A 200 response
        // ignores Range and starts at byte zero, regardless of the request.
        origin = range_start;
        if (range_start == UINT32_MAX ||
            (status_code == 206 && (range_end == UINT32_MAX || range_end < range_start ||
                                   (document_size != UINT32_MAX && range_end >= document_size))))
        {
            read->m_Error = true;
            return;
        }
    }

    uint64_t start = origin + read->m_Received;
    uint64_t end = start + content_data_size;
    read->m_Received += content_data_size;
    if (read->m_Received > UINT32_MAX || origin > read->m_Offset ||
        (status_code == 206 && end > (uint64_t)range_end + 1) ||
        (!read->m_Partial && end > read->m_Capacity))
    {
        read->m_Error = true;
        return;
    }

    // Intersect this chunk with the requested interval, draining bytes outside
    // it without retaining them. All offset arithmetic includes the end byte.
    uint64_t requested_end = (uint64_t)read->m_Offset + read->m_Capacity;
    uint64_t copy_start = dmMath::Max(start, (uint64_t)read->m_Offset);
    uint64_t copy_end = dmMath::Min(end, requested_end);
    if (copy_end > copy_start)
    {
        uint32_t count = (uint32_t)(copy_end - copy_start);
        memcpy(read->m_Buffer + (copy_start - read->m_Offset), (const uint8_t*)content_data + (copy_start - start), count);
        read->m_Copied += count;
    }
}

static dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
{
    HttpRead* read = ((HttpProviderContext*)user_data)->m_Read;
    if (read->m_Partial)
    {
        char value[128];
        dmSnPrintf(value, sizeof(value), "bytes=%u-%u", read->m_Offset, read->m_Offset + read->m_Capacity - 1);
        dmHttpClient::WriteHeader(response, "Range", value);
    }
    return dmHttpClient::RESULT_OK;
}

static bool MatchesUri(const dmURI::Parts* uri)
{
    return strcmp(uri->m_Scheme, "http") == 0 || strcmp(uri->m_Scheme, "https") == 0;
}

static char* CreateEncodedUri(const dmURI::Parts* uri, const char* path, char* buffer, uint32_t buffer_len)
{
    char combined_path[dmResource::RESOURCE_PATH_MAX];
    dmResource::GetCanonicalPathFromBase(uri->m_Path, path, combined_path, sizeof(combined_path));
    dmURI::Encode(combined_path, buffer, buffer_len, 0);
    return buffer;
}

static void ClearResourceIndex(HttpProviderContext* archive)
{
    for (uint32_t i = 0; i < archive->m_ResourceIndex.Size(); ++i)
    {
        free(archive->m_ResourceIndex[i].m_Path);
        free(archive->m_ResourceIndex[i].m_Key);
    }
    archive->m_ResourceIndex.SetSize(0);
    archive->m_ResourceSizes.Clear();
}

static void DeleteHttpArchiveInternal(dmResourceProvider::HArchiveInternal _archive)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    if (archive->m_HttpClient)
        dmHttpClient::Delete(archive->m_HttpClient);

    archive->m_HttpClient = 0;
    ClearResourceIndex(archive);
    delete archive;
}

static void OnCacheEntryVerified(void* user_data, const dmHttpCache::EntryInfo* entry)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;
    if (archive->m_VerifiedEntries.Full())
        archive->m_VerifiedEntries.SetCapacity(archive->m_VerifiedEntries.Capacity() + 128);
    archive->m_VerifiedEntries.Put(dmHashString64(entry->m_URI), entry->m_IdentifierHash);
}

struct ResourceIndexContext
{
    HttpProviderContext* m_Archive;
    char                m_Prefix[dmResource::RESOURCE_PATH_MAX * 2];
    char                m_Line[dmURI::MAX_URI_LEN * 3 + dmHttpCache::MAX_TAG_LEN + 12];
    uint32_t            m_LineSize;
    uint64_t            m_Received;
    int                 m_Status;
    bool                m_Header;
    bool                m_Invalid;
};

static bool ParseIndexLine(ResourceIndexContext* ctx)
{
    ctx->m_Line[ctx->m_LineSize] = '\0';
    if (!ctx->m_Header)
    {
        ctx->m_Header = strcmp(ctx->m_Line, "DM_RESOURCE_INDEX 1") == 0;
        return ctx->m_Header;
    }

    // Records are form-encoded URL path, decimal size and ETag, separated by
    // tabs. Decode once, then use the same URL encoding as resource requests.
    char* size = strchr(ctx->m_Line, '\t');
    char* etag = size ? strchr(size + 1, '\t') : 0;
    if (!size || !etag)
        return false;
    *size++ = '\0';
    *etag++ = '\0';
    if (!size[0] || !etag[0] || strlen(etag) >= dmHttpCache::MAX_TAG_LEN)
        return false;
    for (const char* p = etag; *p; ++p)
        if ((unsigned char)*p < 32 || (unsigned char)*p >= 127)
            return false;

    uint64_t file_size = 0;
    for (const char* p = size; *p; ++p)
    {
        if (*p < '0' || *p > '9')
            return false;
        file_size = file_size * 10 + (*p - '0');
        if (file_size > UINT32_MAX)
            return false;
    }
    for (const char* p = ctx->m_Line; *p; ++p)
    {
        if (*p == '%')
        {
            if (!p[1] || !p[2] || !isxdigit((unsigned char)p[1]) || !isxdigit((unsigned char)p[2]) ||
                (p[1] == '0' && p[2] == '0'))
                return false;
            p += 2;
        }
    }
    char decoded_path[sizeof(ctx->m_Line)];
    dmURI::Decode(ctx->m_Line, decoded_path);
    char encoded_path[dmResource::RESOURCE_PATH_MAX * 2];
    if (decoded_path[0] != '/' || dmURI::Encode(decoded_path, encoded_path, sizeof(encoded_path), 0) != dmURI::RESULT_OK)
        return false;
    if (strncmp(encoded_path, ctx->m_Prefix, strlen(ctx->m_Prefix)) != 0)
        return true;

    HttpProviderContext* archive = ctx->m_Archive;
    char key[dmURI::MAX_URI_LEN];
    if (dmHttpClient::GetURI(archive->m_HttpClient, encoded_path, key, sizeof(key)) != dmHttpClient::RESULT_OK)
        return false;
    dmhash_t key_hash = dmHashString64(key);
    if (archive->m_ResourceSizes.Get(key_hash))
        return false;
    if (archive->m_ResourceSizes.Full())
        archive->m_ResourceSizes.SetCapacity(dmMath::Max(128U, archive->m_ResourceSizes.Capacity() * 2));
    archive->m_ResourceSizes.Put(key_hash, (uint32_t)file_size);
    if (archive->m_ResourceIndex.Full())
        archive->m_ResourceIndex.SetCapacity(dmMath::Max(128U, archive->m_ResourceIndex.Capacity() * 2));
    IndexedResource resource;
    resource.m_Path = strdup(encoded_path);
    resource.m_Key = strdup(key);
    resource.m_KeyHash = key_hash;
    resource.m_Size = (uint32_t)file_size;
    dmStrlCpy(resource.m_ETag, etag, sizeof(resource.m_ETag));
    archive->m_ResourceIndex.Push(resource);

    dmHttpCache::EntryInfo info;
    if (archive->m_HttpCache && dmHttpCache::GetInfo(archive->m_HttpCache, key, &info) == dmHttpCache::RESULT_OK &&
        strcmp(info.m_ETag, etag) == 0 && info.m_RangeStart == 0 && info.m_DocumentSize == file_size)
        OnCacheEntryVerified(archive, &info);
    return true;
}

static void ResourceIndexContent(dmHttpClient::HResponse, void* user_data, int status, const void* data, uint32_t size,
                                 int32_t, uint32_t, uint32_t, uint32_t, const char* method)
{
    ResourceIndexContext* ctx = (ResourceIndexContext*)user_data;
    ctx->m_Status = status;
    if (!data && !size && !method)
    {
        ctx->m_LineSize = 0;
        ctx->m_Received = 0;
        ctx->m_Header = false;
        ctx->m_Invalid = false;
        ClearResourceIndex(ctx->m_Archive);
        ctx->m_Archive->m_VerifiedEntries.Clear();
    }
    ctx->m_Received += size;
    if (ctx->m_Received > 64U * 1024U * 1024U)
        ctx->m_Invalid = true;
    if (status != 200 || ctx->m_Invalid)
        return;
    const char* bytes = (const char*)data;
    for (uint32_t i = 0; i < size && !ctx->m_Invalid; ++i)
    {
        if (bytes[i] == '\n')
        {
            ctx->m_Invalid = !ParseIndexLine(ctx);
            ctx->m_LineSize = 0;
        }
        else if (bytes[i] && ctx->m_LineSize < sizeof(ctx->m_Line) - 1)
            ctx->m_Line[ctx->m_LineSize++] = bytes[i];
        else
            ctx->m_Invalid = true;
    }
}

static int CompareResourceSize(const void* a, const void* b)
{
    const IndexedResource* first = (const IndexedResource*)a;
    const IndexedResource* second = (const IndexedResource*)b;
    if (first->m_Size != second->m_Size)
        return first->m_Size < second->m_Size ? -1 : 1;
    return strcmp(first->m_Path, second->m_Path);
}

static bool LoadResourceIndex(HttpProviderContext* archive)
{
    ResourceIndexContext ctx = {};
    ctx.m_Archive = archive;
    CreateEncodedUri(&archive->m_BaseUri, "/", ctx.m_Prefix, sizeof(ctx.m_Prefix));
    dmHttpClient::NewParams params;
    params.m_HttpContent = ResourceIndexContent;
    params.m_Userdata = &ctx;
    params.m_RequestTimeout = 5 * 1000 * 1000;
    dmHttpClient::HClient client = dmHttpClient::New(&params, &archive->m_BaseUri);
    if (!client)
        return false;
    dmHttpClient::Result result = dmHttpClient::Get(client, "/__resource_index__");
    dmHttpClient::Delete(client);
    if (result == dmHttpClient::RESULT_OK && ctx.m_Status == 200 && ctx.m_Header && !ctx.m_Invalid && !ctx.m_LineSize)
    {
        if (archive->m_ResourceIndex.Size() > 1)
            qsort(archive->m_ResourceIndex.Begin(), archive->m_ResourceIndex.Size(), sizeof(IndexedResource), CompareResourceSize);
        return true;
    }

    // A partial or invalid index must grant neither size metadata nor cache
    // trust. Generic HTTP servers retain ordinary HEAD/GET and verification.
    ClearResourceIndex(archive);
    archive->m_VerifiedEntries.Clear();
    return false;
}

struct ResourceBatchContext
{
    HttpProviderContext*             m_Archive;
    dmArray<const IndexedResource*> m_Resources;
    dmArray<char>                   m_Request;
    dmHttpCache::HCacheCreator       m_Creator;
    uint32_t                        m_PayloadSize;
    uint32_t                        m_Entry;
    uint32_t                        m_Remaining;
    uint32_t                        m_LineSize;
    char                            m_Line[dmURI::MAX_URI_LEN + dmHttpCache::MAX_TAG_LEN + 12];
    int                             m_Status;
    bool                            m_Header;
    bool                            m_Invalid;
};

static bool AddBatchResource(ResourceBatchContext* ctx, const IndexedResource* resource)
{
    uint32_t path_size = strlen(resource->m_Path);
    uint32_t tag_size = strlen(resource->m_ETag);
    uint32_t record_size = path_size + tag_size + 2;
    if (ctx->m_Resources.Size() == MAX_BATCH_FILES || ctx->m_PayloadSize + resource->m_Size > MAX_BATCH_SIZE ||
        ctx->m_Request.Size() + record_size > MAX_BATCH_REQUEST_SIZE)
        return false;
    if (ctx->m_Resources.Full())
        ctx->m_Resources.SetCapacity(dmMath::Max(64U, ctx->m_Resources.Capacity() * 2));
    ctx->m_Resources.Push(resource);
    ctx->m_PayloadSize += resource->m_Size;
    if (ctx->m_Request.Remaining() < record_size)
        ctx->m_Request.OffsetCapacity(dmMath::Max(record_size, 16U * 1024U));
    uint32_t offset = ctx->m_Request.Size();
    ctx->m_Request.SetSize(offset + record_size);
    char* out = ctx->m_Request.Begin() + offset;
    memcpy(out, resource->m_Path, path_size);
    out[path_size] = '\t';
    memcpy(out + path_size + 1, resource->m_ETag, tag_size);
    out[record_size - 1] = '\n';
    return true;
}

static void AbortBatchEntry(ResourceBatchContext* ctx)
{
    if (ctx->m_Creator)
    {
        dmHttpCache::SetError(ctx->m_Archive->m_HttpCache, ctx->m_Creator);
        dmHttpCache::End(ctx->m_Archive->m_HttpCache, ctx->m_Creator);
        ctx->m_Creator = 0;
    }
}

static void FinishBatchEntry(ResourceBatchContext* ctx)
{
    HttpProviderContext* archive = ctx->m_Archive;
    if (ctx->m_Creator)
    {
        dmHttpCache::Result result = dmHttpCache::End(archive->m_HttpCache, ctx->m_Creator);
        ctx->m_Creator = 0;
        if (result != dmHttpCache::RESULT_OK)
        {
            ctx->m_Invalid = true;
            return;
        }
    }
    const IndexedResource* resource = ctx->m_Resources[ctx->m_Entry];
    dmHttpCache::EntryInfo info;
    if (dmHttpCache::GetInfo(archive->m_HttpCache, resource->m_Key, &info) != dmHttpCache::RESULT_OK ||
        strcmp(info.m_ETag, resource->m_ETag) != 0 || info.m_RangeStart != 0 || info.m_DocumentSize != resource->m_Size)
    {
        ctx->m_Invalid = true;
        return;
    }
    OnCacheEntryVerified(archive, &info);
    ++ctx->m_Entry;
}

static void ParseBatchHeader(ResourceBatchContext* ctx)
{
    ctx->m_Line[ctx->m_LineSize] = '\0';
    if (!ctx->m_Header)
    {
        ctx->m_Header = strcmp(ctx->m_Line, "DM_RESOURCE_BATCH 1") == 0;
        ctx->m_Invalid = !ctx->m_Header;
        return;
    }
    if (ctx->m_Entry == ctx->m_Resources.Size())
    {
        ctx->m_Invalid = true;
        return;
    }
    const IndexedResource* resource = ctx->m_Resources[ctx->m_Entry];
    char expected[sizeof(ctx->m_Line)];
    dmSnPrintf(expected, sizeof(expected), "%s\t%u\t%s", resource->m_Path, resource->m_Size, resource->m_ETag);
    if (strcmp(expected, ctx->m_Line) != 0)
    {
        ctx->m_Invalid = true;
        return;
    }
    dmHttpCache::Result result = dmHttpCache::Begin(ctx->m_Archive->m_HttpCache, resource->m_Key, resource->m_ETag, 0,
                                                  0, resource->m_Size ? resource->m_Size - 1 : 0, resource->m_Size, &ctx->m_Creator);
    if (result != dmHttpCache::RESULT_OK && result != dmHttpCache::RESULT_ALREADY_CACHED)
    {
        ctx->m_Invalid = true;
        return;
    }
    ctx->m_Remaining = resource->m_Size;
    if (!ctx->m_Remaining)
        FinishBatchEntry(ctx);
}

static void ResourceBatchContent(dmHttpClient::HResponse, void* user_data, int status, const void* data, uint32_t size,
                                 int32_t, uint32_t, uint32_t, uint32_t, const char* method)
{
    ResourceBatchContext* ctx = (ResourceBatchContext*)user_data;
    ctx->m_Status = status;
    if (!data && !size && !method)
    {
        AbortBatchEntry(ctx);
        ctx->m_Entry = 0;
        ctx->m_Remaining = 0;
        ctx->m_LineSize = 0;
        ctx->m_Header = false;
        ctx->m_Invalid = false;
    }
    if (status != 200)
        return;
    const char* bytes = (const char*)data;
    while (size && !ctx->m_Invalid)
    {
        if (ctx->m_Remaining)
        {
            uint32_t count = dmMath::Min(size, ctx->m_Remaining);
            if (ctx->m_Creator && dmHttpCache::Add(ctx->m_Archive->m_HttpCache, ctx->m_Creator, bytes, count) != dmHttpCache::RESULT_OK)
            {
                ctx->m_Invalid = true;
                break;
            }
            bytes += count;
            size -= count;
            ctx->m_Remaining -= count;
            if (!ctx->m_Remaining)
                FinishBatchEntry(ctx);
        }
        else
        {
            if (*bytes == '\n')
            {
                ParseBatchHeader(ctx);
                ctx->m_LineSize = 0;
            }
            else if (*bytes && ctx->m_LineSize < sizeof(ctx->m_Line) - 1)
                ctx->m_Line[ctx->m_LineSize++] = *bytes;
            else
                ctx->m_Invalid = true;
            ++bytes;
            --size;
        }
    }
}

static uint32_t BatchContentLength(dmHttpClient::HResponse, void* user_data)
{
    return ((ResourceBatchContext*)user_data)->m_Request.Size();
}

static dmHttpClient::Result WriteBatchRequest(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* user_data)
{
    return dmHttpClient::Write(response, ((ResourceBatchContext*)user_data)->m_Request.Begin() + offset, length);
}

static void FetchResourceBatch(HttpProviderContext* archive, dmhash_t requested_key)
{
    ResourceBatchContext ctx = {};
    ctx.m_Archive = archive;
    for (uint32_t i = 0; i < archive->m_ResourceIndex.Size(); ++i)
    {
        const IndexedResource* resource = &archive->m_ResourceIndex[i];
        if (resource->m_Size > MAX_BATCH_FILE_SIZE)
            break;
        if (resource->m_KeyHash == requested_key)
        {
            AddBatchResource(&ctx, resource);
            break;
        }
    }
    if (!ctx.m_Resources.Size())
        return;

    // Lead with the requested resource, then fill the payload budget from the
    // smallest uncached resources. Invalidated metadata cannot drive prefetch.
    for (uint32_t i = 0; i < archive->m_ResourceIndex.Size(); ++i)
    {
        const IndexedResource* resource = &archive->m_ResourceIndex[i];
        if (resource->m_Size > MAX_BATCH_FILE_SIZE)
            break;
        if (resource->m_KeyHash != requested_key && archive->m_ResourceSizes.Get(resource->m_KeyHash) &&
            !archive->m_VerifiedEntries.Get(resource->m_KeyHash) && !AddBatchResource(&ctx, resource))
            break;
    }

    dmHttpClient::NewParams params;
    params.m_HttpSendContentLength = BatchContentLength;
    params.m_HttpWrite = WriteBatchRequest;
    params.m_HttpContent = ResourceBatchContent;
    params.m_Userdata = &ctx;
    params.m_RequestTimeout = 30 * 1000 * 1000;
    dmHttpClient::HClient client = dmHttpClient::New(&params, &archive->m_BaseUri);
    if (!client)
        return;
    dmHttpClient::Result result = dmHttpClient::Post(client, "/__resource_batch__");
    dmHttpClient::Delete(client);
    AbortBatchEntry(&ctx);
    // Complete entries remain usable if a later entry failed. The incomplete
    // entry is discarded, and future misses use individual resource requests.
    if (result != dmHttpClient::RESULT_OK || ctx.m_Status != 200 || ctx.m_Invalid ||
        !ctx.m_Header || ctx.m_LineSize || ctx.m_Entry != ctx.m_Resources.Size())
        archive->m_BatchSupported = false;
}

static dmHttpClient::Result RequestVerified(HttpProviderContext* archive, const char* method, const char* cache_key)
{
    dmhash_t key_hash = dmHashString64(cache_key);
    uint64_t* verified = archive->m_VerifiedEntries.Get(key_hash);
    dmHttpClient::Result result = dmHttpClient::RESULT_IO_ERROR;
    if (verified)
    {
        dmHttpCache::EntryInfo info;
        if (dmHttpCache::GetInfo(archive->m_HttpCache, cache_key, &info) == dmHttpCache::RESULT_OK && info.m_IdentifierHash == *verified)
            result = dmHttpClient::RequestCached(archive->m_HttpClient, method, info.m_ETag);
        if (result != dmHttpClient::RESULT_NOT_200_OK)
            archive->m_VerifiedEntries.Erase(key_hash);
    }
    return result;
}

static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
{
    if (!MatchesUri(uri))
        return dmResourceProvider::RESULT_NOT_SUPPORTED;

    HttpProviderContext* archive = new HttpProviderContext;
    memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));
    archive->m_HttpCache = g_HttpCache;
    archive->m_Read = 0;
    archive->m_BatchSupported = true;

    dmHttpClient::NewParams http_params;
    http_params.m_HttpContent = &HttpContent;
    http_params.m_HttpWriteHeaders = &HttpWriteHeaders;
    http_params.m_Userdata = archive;
    http_params.m_HttpCache = archive->m_HttpCache;
    archive->m_HttpClient = dmHttpClient::New(&http_params, uri, 0);
    if (!archive->m_HttpClient)
    {
        char buffer[dmResource::RESOURCE_PATH_MAX*2];
        dmLogError("Failed to connect to: %s", CreateEncodedUri(uri, "", buffer, sizeof(buffer)));
        DeleteHttpArchiveInternal(archive);
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    if (!LoadResourceIndex(archive) && archive->m_HttpCache)
    {
        char path[dmResource::RESOURCE_PATH_MAX * 2];
        CreateEncodedUri(uri, "/", path, sizeof(path));
        char prefix[dmURI::MAX_URI_LEN];
        if (dmHttpClient::GetURI(archive->m_HttpClient, path, prefix, sizeof(prefix)) == dmHttpClient::RESULT_OK &&
            dmStrlCat(prefix, prefix[strlen(prefix) - 1] == '/' ? "" : "/", sizeof(prefix)) < sizeof(prefix))
        {
            // Only this mount may trust these snapshots. Ordinary HTTP clients
            // sharing the cache retain their normal revalidation policy.
            dmHttpCacheVerify::VerifyCache(archive->m_HttpCache, &archive->m_BaseUri, 5 * 24 * 60 * 60,
                                           prefix, OnCacheEntryVerified, archive);
        }
    }

    *out_archive = (dmResourceProvider::HArchiveInternal)archive;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
{
    DeleteHttpArchiveInternal((HttpProviderContext*)archive);
    return dmResourceProvider::RESULT_OK;
}

// The resource mount mutex/preloader queue serializes these synchronous reads.
// On failure the destination may contain a prefix; callers must discard it.
static dmResourceProvider::Result GetRequestFromUri(HttpProviderContext* archive, const char* method, const char* path,
                                                   uint32_t offset, uint32_t capacity, bool partial, uint8_t* buffer, uint32_t* nread)
{
    *nread = 0;
    if (partial && !capacity)
        return dmResourceProvider::RESULT_OK;
    if ((uint64_t)offset + capacity > (uint64_t)UINT32_MAX + 1)
        return dmResourceProvider::RESULT_IO_ERROR;

    HttpRead read = {};
    read.m_Buffer = buffer;
    read.m_Capacity = capacity;
    read.m_Offset = offset;
    read.m_Partial = partial;
    read.m_Head = strcmp(method, "HEAD") == 0;
    read.m_ContentLength = -1;
    read.m_Status = -1;

    char encoded_uri[dmResource::RESOURCE_PATH_MAX*2];
    CreateEncodedUri(&archive->m_BaseUri, path, encoded_uri, sizeof(encoded_uri));

    char cache_key[dmURI::MAX_URI_LEN];
    if (dmHttpClient::GetURI(archive->m_HttpClient, encoded_uri, cache_key, sizeof(cache_key)) != dmHttpClient::RESULT_OK)
        return dmResourceProvider::RESULT_INVAL_ERROR;

    dmhash_t file_key_hash = dmHashString64(cache_key);
    uint32_t* indexed_size = archive->m_ResourceSizes.Get(file_key_hash);
    if (read.m_Head && indexed_size)
    {
        *nread = *indexed_size;
        return dmResourceProvider::RESULT_OK;
    }

    if (partial)
    {
        char range[128];
        dmSnPrintf(range, sizeof(range), "=bytes=%u-%u", offset, offset + capacity - 1);
        if (dmStrlCat(cache_key, range, sizeof(cache_key)) >= sizeof(cache_key))
            return dmResourceProvider::RESULT_INVAL_ERROR;
    }
    dmHttpClient::SetCacheKey(archive->m_HttpClient, cache_key);

    archive->m_Read = &read;
    dmHttpClient::Result http_result = RequestVerified(archive, method, cache_key);
    if (http_result != dmHttpClient::RESULT_NOT_200_OK && !partial && !read.m_Head && indexed_size &&
        *indexed_size <= MAX_BATCH_FILE_SIZE && archive->m_BatchSupported && archive->m_HttpCache)
    {
        FetchResourceBatch(archive, file_key_hash);
        http_result = RequestVerified(archive, method, cache_key);
    }
    if (http_result != dmHttpClient::RESULT_NOT_200_OK)
        http_result = dmHttpClient::Request(archive->m_HttpClient, method, encoded_uri);
    archive->m_Read = 0;

    if (indexed_size && (http_result == dmHttpClient::RESULT_OK || http_result == dmHttpClient::RESULT_NOT_200_OK) &&
        !partial && !read.m_Head && read.m_Received != *indexed_size)
        read.m_Error = true;
    if (indexed_size && (read.m_Error || (read.m_Status != 200 && read.m_Status != 206 && read.m_Status != 304)))
    {
        archive->m_ResourceSizes.Erase(file_key_hash);
        if (archive->m_VerifiedEntries.Get(file_key_hash))
            archive->m_VerifiedEntries.Erase(file_key_hash);
    }

    bool http_result_ok = http_result == dmHttpClient::RESULT_OK ||
                         (http_result == dmHttpClient::RESULT_NOT_200_OK &&
                                (read.m_Status == 304 || read.m_Status == 206));

    if (!http_result_ok)
    {
        if (read.m_Status == 404)
        {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }
        else
        {
            // 304 (NOT MODIFIED) is OK. 304 is returned when the resource is loaded from cache, ie ETag or similar match
            if (http_result == dmHttpClient::RESULT_NOT_200_OK && read.m_Status != 304)
            {
                dmLogWarning("Unexpected http status code: %d", read.m_Status);
                return dmResourceProvider::RESULT_IO_ERROR;
            }

            dmLogError("Unexpected http result: %d %s", http_result, dmHttpClient::ResultToString(http_result));
            return dmResourceProvider::RESULT_IO_ERROR;
        }
    }

    if (read.m_Error)
        return dmResourceProvider::RESULT_IO_ERROR;
    if (read.m_Head)
    {
        if (read.m_ContentLength < 0)
            return dmResourceProvider::RESULT_IO_ERROR;
        *nread = (uint32_t)read.m_ContentLength;
    }
    else
    {
        if (read.m_ContentLength >= 0 && (uint64_t)read.m_ContentLength != read.m_Received)
            return dmResourceProvider::RESULT_IO_ERROR;
        *nread = read.m_Copied;
    }
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;

    return GetRequestFromUri((HttpProviderContext*)archive, "HEAD", path, 0, 0, false, 0, file_size);
}

static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t _buffer_len)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;

    uint32_t buffer_len = _buffer_len;
    dmResourceProvider::Result result = GetRequestFromUri((HttpProviderContext*)archive, "GET", path, 0, _buffer_len, false, buffer, &buffer_len);
    if (result != dmResourceProvider::RESULT_OK)
    {
        return result;
    }
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result ReadFilePartial(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;
    dmResourceProvider::Result result = GetRequestFromUri((HttpProviderContext*)archive, "GET", path, offset, size, true, buffer, nread);
    if (result != dmResourceProvider::RESULT_OK)
    {
        return result;
    }
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result InitializeArchiveLoaderHttp(dmResourceProvider::ArchiveLoaderParams* params, dmResourceProvider::ArchiveLoader* loader)
{
    g_HttpCache = params->m_HttpCache;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result FinalizeArchiveLoaderHttp(dmResourceProvider::ArchiveLoaderParams* params, dmResourceProvider::ArchiveLoader* loader)
{
    g_HttpCache = 0;
    return dmResourceProvider::RESULT_OK;
}

static void InvalidateFileCache(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path)
{
    (void)path_hash;
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    char encoded_uri[dmResource::RESOURCE_PATH_MAX * 2];
    CreateEncodedUri(&archive->m_BaseUri, path, encoded_uri, sizeof(encoded_uri));
    char key[dmURI::MAX_URI_LEN];
    if (dmHttpClient::GetURI(archive->m_HttpClient, encoded_uri, key, sizeof(key)) == dmHttpClient::RESULT_OK)
    {
        dmhash_t key_hash = dmHashString64(key);
        if (archive->m_VerifiedEntries.Get(key_hash))
            archive->m_VerifiedEntries.Erase(key_hash);
        if (archive->m_ResourceSizes.Get(key_hash))
            archive->m_ResourceSizes.Erase(key_hash);
    }
}

static void SetupArchiveLoaderHttp(dmResourceProvider::ArchiveLoader* loader)
{
    loader->m_CanMount          = MatchesUri;
    loader->m_Mount             = Mount;
    loader->m_Unmount           = Unmount;
    loader->m_GetFileSize       = GetFileSize;
    loader->m_ReadFile          = ReadFile;
    loader->m_ReadFilePartial   = ReadFilePartial;
    loader->m_InvalidateFileCache = InvalidateFileCache;
}

DM_DECLARE_ARCHIVE_LOADER(ResourceProviderHttp, "http", SetupArchiveLoaderHttp, InitializeArchiveLoaderHttp, FinalizeArchiveLoaderHttp);
}
