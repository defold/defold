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

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include <dlib/http/http_client.h>
#include <dlib/http/http_cache.h>
#include <dlib/http_cache_verify.h>

#include <stdio.h> // debug printf

namespace dmResourceProviderHttp
{

static dmHttpCache::HCache g_HttpCache = 0;

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

static void DeleteHttpArchiveInternal(dmResourceProvider::HArchiveInternal _archive)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    if (archive->m_HttpClient)
        dmHttpClient::Delete(archive->m_HttpClient);

    archive->m_HttpClient = 0;
    delete archive;
}

static void OnCacheEntryVerified(void* user_data, const dmHttpCache::EntryInfo* entry)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;
    if (archive->m_VerifiedEntries.Full())
        archive->m_VerifiedEntries.SetCapacity(archive->m_VerifiedEntries.Capacity() + 128);
    archive->m_VerifiedEntries.Put(dmHashString64(entry->m_URI), entry->m_IdentifierHash);
}

static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
{
    if (!MatchesUri(uri))
        return dmResourceProvider::RESULT_NOT_SUPPORTED;

    HttpProviderContext* archive = new HttpProviderContext;
    memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));
    archive->m_HttpCache = g_HttpCache;
    archive->m_Read = 0;

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

    if (archive->m_HttpCache)
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

    if (partial)
    {
        char range[128];
        dmSnPrintf(range, sizeof(range), "=bytes=%u-%u", offset, offset + capacity - 1);
        if (dmStrlCat(cache_key, range, sizeof(cache_key)) >= sizeof(cache_key))
            return dmResourceProvider::RESULT_INVAL_ERROR;
    }
    dmHttpClient::SetCacheKey(archive->m_HttpClient, cache_key);

    archive->m_Read = &read;
    dmHttpClient::Result http_result = dmHttpClient::RESULT_IO_ERROR;
    uint64_t* verified = archive->m_VerifiedEntries.Get(dmHashString64(cache_key));
    if (verified)
    {
        dmHttpCache::EntryInfo info;
        if (dmHttpCache::GetInfo(archive->m_HttpCache, cache_key, &info) == dmHttpCache::RESULT_OK &&
            info.m_IdentifierHash == *verified)
            http_result = dmHttpClient::RequestCached(archive->m_HttpClient, method, info.m_ETag);
        if (http_result != dmHttpClient::RESULT_NOT_200_OK)
            archive->m_VerifiedEntries.Erase(dmHashString64(cache_key));
    }
    if (http_result != dmHttpClient::RESULT_NOT_200_OK)
        http_result = dmHttpClient::Request(archive->m_HttpClient, method, encoded_uri);
    archive->m_Read = 0;

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
