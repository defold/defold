// Copyright 2020-2025 The Defold Foundation
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
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/http_cache.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include <dlib/http_client.h>
#include <dlib/http_cache.h>
#include <dlib/http_cache_verify.h>

#include <stdio.h> // debug printf

namespace dmResourceProviderHttp
{

static const uint32_t INVALID_CHUNK_VALUE = 0xFFFFFFFF;

static dmHttpCache::HCache g_HttpCache = 0;

struct HttpProviderContext
{
    dmURI::Parts            m_BaseUri;
    dmHttpClient::HClient   m_HttpClient;
    dmHttpCache::HCache     m_HttpCache;
    dmArray<char>           m_HttpBuffer;

    int32_t                 m_HttpContentLength;        // Total number bytes loaded in current GET-request
    int32_t                 m_HttpContentOffset;
    uint32_t                m_HttpTotalBytesStreamed;
    int                     m_HttpStatus;

    // when sending
    uint32_t                m_ChunkOffset;
    uint32_t                m_ChunkSize;
    uint32_t                m_ChunkedRequest:1;
    uint32_t                :31;
};

static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;
    archive->m_HttpStatus = status_code;

    if (dmStrCaseCmp(key, "Content-Length") == 0)
    {
        archive->m_HttpContentLength = strtol(value, 0, 10);
        if (archive->m_HttpContentLength < 0) {
            dmLogError("Content-Length negative (%d)", archive->m_HttpContentLength);
        } else {
            if (archive->m_HttpBuffer.Capacity() < (uint32_t)archive->m_HttpContentLength) {
                archive->m_HttpBuffer.SetCapacity(archive->m_HttpContentLength);
            }
            archive->m_HttpBuffer.SetSize(0);
        }
    }
    else if (dmStrCaseCmp(key, "Content-Range") == 0)
    {
        int start, end, size;
        int count = sscanf(value, "bytes %d-%d/%d", &start, &end, &size);
        if (count == 3)
        {
            archive->m_HttpContentOffset = (uint32_t)start;

            archive->m_HttpContentLength = uint32_t(end - start);
            if (archive->m_HttpBuffer.Capacity() < (uint32_t)archive->m_HttpContentLength) {
                archive->m_HttpBuffer.SetCapacity(archive->m_HttpContentLength);
            }
            archive->m_HttpBuffer.SetSize(0);
        }
    }
}

static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                        uint32_t range_start, uint32_t range_end, uint32_t document_size,
                        const char* method)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;
    (void) status_code;
    (void) method;

    if (!content_data && content_data_size)
    {
        archive->m_HttpBuffer.SetSize(0);
        return;
    }

    // CHECK: Content-Range: bytes 200-1000/67589

    // We must set http-status here. For direct cached result HttpHeader is not called.
    archive->m_HttpStatus = status_code;

    if (archive->m_HttpBuffer.Remaining() < content_data_size) {
        uint32_t diff = content_data_size - archive->m_HttpBuffer.Remaining();
        // NOTE: Resizing the the array can be inefficient but sometimes we don't know the actual size, i.e. when "Content-Size" isn't set
        archive->m_HttpBuffer.OffsetCapacity(diff + 1024 * 1024);
    }

    archive->m_HttpBuffer.PushArray((const char*) content_data, content_data_size);
    archive->m_HttpTotalBytesStreamed += content_data_size;
    archive->m_HttpContentLength = content_length;
}

static dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;

    if (archive->m_ChunkedRequest)
    {
        // E.g. "Range: bytes=0-1023"
        char value[128];
        dmSnPrintf(value, sizeof(value), "bytes=%u-%u", archive->m_ChunkOffset, archive->m_ChunkOffset+archive->m_ChunkSize-1);
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

static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
{
    if (!MatchesUri(uri))
        return dmResourceProvider::RESULT_NOT_SUPPORTED;

    HttpProviderContext* archive = new HttpProviderContext;
    memset(archive, 0, sizeof(HttpProviderContext));
    memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));
    archive->m_HttpCache = g_HttpCache;

    dmHttpClient::NewParams http_params;
    http_params.m_HttpHeader = &HttpHeader;
    http_params.m_HttpContent = &HttpContent;
    http_params.m_HttpWriteHeaders = &HttpWriteHeaders;
    http_params.m_Userdata = archive;
    http_params.m_HttpCache = archive->m_HttpCache;
    archive->m_HttpClient = dmHttpClient::New(&http_params, uri->m_Hostname, uri->m_Port, strcmp(uri->m_Scheme, "https") == 0, 0);
    if (!archive->m_HttpClient)
    {
        char buffer[dmResource::RESOURCE_PATH_MAX*2];
        dmLogError("Failed to connect to: %s", CreateEncodedUri(uri, "", buffer, sizeof(buffer)));
        DeleteHttpArchiveInternal(archive);
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    *out_archive = (dmResourceProvider::HArchiveInternal)archive;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
{
    DeleteHttpArchiveInternal((HttpProviderContext*)archive);
    return dmResourceProvider::RESULT_OK;
}

static void ResetHttpInfo(HttpProviderContext* archive)
{
    archive->m_HttpContentLength = -1;
    archive->m_HttpTotalBytesStreamed = 0;
    archive->m_HttpStatus = -1;
    archive->m_HttpBuffer.SetSize(0);
    archive->m_HttpContentOffset = 0;
    archive->m_ChunkOffset      = INVALID_CHUNK_VALUE;
    archive->m_ChunkSize        = INVALID_CHUNK_VALUE;
    archive->m_ChunkedRequest   = 0;
}

// Note. This is used in a synchronous manner.
// We rely on the fact that the resource_preloader uses a threaded queue to make requests
static dmResourceProvider::Result GetRequestFromUri(HttpProviderContext* archive, const char* method, const char* path, uint32_t offset, uint32_t size, uint32_t* buffer_length, uint8_t* buffer)
{
    ResetHttpInfo(archive);

    if (size != INVALID_CHUNK_VALUE && offset != INVALID_CHUNK_VALUE)
    {
        archive->m_ChunkOffset      = offset;
        archive->m_ChunkSize        = size;
        archive->m_ChunkedRequest   = 1;
    }

    bool get_file_size = strcmp(method, "HEAD") == 0;

    // // Always verify cache for reloaded resources
    // if (factory->m_HttpCache)
    //     dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);

    char encoded_uri[dmResource::RESOURCE_PATH_MAX*2];
    CreateEncodedUri(&archive->m_BaseUri, path, encoded_uri, sizeof(encoded_uri));

    char cache_key[dmURI::MAX_URI_LEN];
    dmHttpClient::GetURI(archive->m_HttpClient, path, cache_key, sizeof(cache_key));

    // Make the cache key the same (see http_service.cpp)
    if (!get_file_size)
    {
        if(INVALID_CHUNK_VALUE != offset && INVALID_CHUNK_VALUE != size)
        {
            char range[256];
            dmSnPrintf(range, sizeof(range), "=bytes=%u-%u", offset, offset+size-1);
            dmStrlCat(cache_key, range, sizeof(cache_key));// "=bytes=%d-%d"
        }
        dmHttpClient::SetCacheKey(archive->m_HttpClient, cache_key);
    }

    dmHttpClient::Result http_result = dmHttpClient::Request(archive->m_HttpClient, method, encoded_uri);

    // // Always verify cache for reloaded resources
    // if (factory->m_HttpCache)
    //     dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUSTED);

    bool http_result_ok = http_result == dmHttpClient::RESULT_OK ||
                         (http_result == dmHttpClient::RESULT_NOT_200_OK &&
                                (archive->m_HttpStatus == 304 || archive->m_HttpStatus == 206));

    if (!http_result_ok)
    {
        if (archive->m_HttpStatus == 404)
        {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }
        else
        {
            // 304 (NOT MODIFIED) is OK. 304 is returned when the resource is loaded from cache, ie ETag or similar match
            if (http_result == dmHttpClient::RESULT_NOT_200_OK && archive->m_HttpStatus != 304)
            {
                dmLogWarning("Unexpected http status code: %d", archive->m_HttpStatus);
                return dmResourceProvider::RESULT_IO_ERROR;
            }

            dmLogError("Unexpected http result: %d %s", http_result, dmHttpClient::ResultToString(http_result));
            return dmResourceProvider::RESULT_IO_ERROR;
        }
    }

    if (get_file_size)
    {
        *buffer_length = archive->m_HttpContentLength;
    }
    else
    {
        // Only check content-length if status != 304 (NOT MODIFIED)
        if (archive->m_HttpStatus != 304 && archive->m_HttpContentLength != -1 && archive->m_HttpContentLength != (int32_t)archive->m_HttpTotalBytesStreamed)
        {
            dmLogError("Expected content length differs from actually streamed for resource %s (%d != %d)", encoded_uri, archive->m_HttpContentLength, archive->m_HttpTotalBytesStreamed);
        }

        // We might have streamed more than we have a buffer for
        // If we got more than requested, and we wanted the full file, let's fail for now.
        bool full_file = size == INVALID_CHUNK_VALUE && offset == INVALID_CHUNK_VALUE;
        if (archive->m_HttpTotalBytesStreamed > *buffer_length && full_file)
        {
            dmLogError("Cannot write to buffer of size %u. Content has length %u", *buffer_length, archive->m_HttpTotalBytesStreamed);
            return dmResourceProvider::RESULT_IO_ERROR;
        }

        *buffer_length = dmMath::Min(archive->m_HttpTotalBytesStreamed, size);
        if (buffer)
        {
            memcpy(buffer, archive->m_HttpBuffer.Begin(), *buffer_length);
        }
    }
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;

    return GetRequestFromUri((HttpProviderContext*)archive, "HEAD", path, INVALID_CHUNK_VALUE, INVALID_CHUNK_VALUE, file_size, 0);
}

static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t _buffer_len)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;

    uint32_t buffer_len = _buffer_len;
    dmResourceProvider::Result result = GetRequestFromUri((HttpProviderContext*)archive, "GET", path, INVALID_CHUNK_VALUE, INVALID_CHUNK_VALUE, &buffer_len, buffer);
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
    dmResourceProvider::Result result = GetRequestFromUri((HttpProviderContext*)archive, "GET", path, offset, size, nread, buffer);
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

static void SetupArchiveLoaderHttp(dmResourceProvider::ArchiveLoader* loader)
{
    loader->m_CanMount          = MatchesUri;
    loader->m_Mount             = Mount;
    loader->m_Unmount           = Unmount;
    loader->m_GetFileSize       = GetFileSize;
    loader->m_ReadFile          = ReadFile;
    loader->m_ReadFilePartial   = ReadFilePartial;
}

DM_DECLARE_ARCHIVE_LOADER(ResourceProviderHttp, "http", SetupArchiveLoaderHttp, InitializeArchiveLoaderHttp, FinalizeArchiveLoaderHttp);
}
