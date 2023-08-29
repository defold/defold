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

#include "provider.h"
#include "provider_private.h"
#include "../resource_util.h"

#include <dlib/dstrings.h>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/sys.h>

#include <dlib/http_client.h>
#include <dlib/http_cache.h>
#include <dlib/http_cache_verify.h>

#include <stdio.h> // debug printf

namespace dmResourceProviderHttp
{

struct HttpProviderContext
{
    dmURI::Parts            m_BaseUri;
    dmHttpClient::HClient   m_HttpClient;
    dmHttpCache::HCache     m_HttpCache;
    dmArray<char>           m_HttpBuffer;

    int32_t                 m_HttpContentLength;        // Total number bytes loaded in current GET-request
    uint32_t                m_HttpTotalBytesStreamed;
    int                     m_HttpStatus;
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
}

static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
{
    HttpProviderContext* archive = (HttpProviderContext*)user_data;
    (void) status_code;

    if (!content_data && content_data_size)
    {
        archive->m_HttpBuffer.SetSize(0);
        return;
    }

    // We must set http-status here. For direct cached result HttpHeader is not called.
    archive->m_HttpStatus = status_code;

    if (archive->m_HttpBuffer.Remaining() < content_data_size) {
        uint32_t diff = content_data_size - archive->m_HttpBuffer.Remaining();
        // NOTE: Resizing the the array can be inefficient but sometimes we don't know the actual size, i.e. when "Content-Size" isn't set
        archive->m_HttpBuffer.OffsetCapacity(diff + 1024 * 1024);
    }

    archive->m_HttpBuffer.PushArray((const char*) content_data, content_data_size);
    archive->m_HttpTotalBytesStreamed += content_data_size;
}

static bool MatchesUri(const dmURI::Parts* uri)
{
    return strcmp(uri->m_Scheme, "http") == 0 || strcmp(uri->m_Scheme, "https") == 0;
}

static char* CreateEncodedUri(const dmURI::Parts* uri, const char* path, char* buffer, uint32_t buffer_len)
{
    char combined_path[dmResource::RESOURCE_PATH_MAX];
    dmResource::GetCanonicalPathFromBase(uri->m_Path, path, combined_path);
    dmURI::Encode(combined_path, buffer, buffer_len, 0);
    return buffer;
}

static void DeleteHttpArchiveInternal(dmResourceProvider::HArchiveInternal _archive)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    if (archive->m_HttpClient)
        dmHttpClient::Delete(archive->m_HttpClient);
    if (archive->m_HttpCache)
        dmHttpCache::Close(archive->m_HttpCache);

    archive->m_HttpClient = 0;
    archive->m_HttpCache = 0;
    delete archive;
}

static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
{
    if (!MatchesUri(uri))
        return dmResourceProvider::RESULT_NOT_SUPPORTED;

    HttpProviderContext* archive = new HttpProviderContext;
    memset(archive, 0, sizeof(HttpProviderContext));
    memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));

    dmHttpClient::NewParams http_params;
    http_params.m_HttpHeader = &HttpHeader;
    http_params.m_HttpContent = &HttpContent;
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
}

// Note. This is used in a synchronous manner.
static dmResourceProvider::Result GetRequestFromUri(HttpProviderContext* archive, const char* method, const char* path, uint32_t* buffer_length, uint8_t* buffer)
{
    ResetHttpInfo(archive);

    // // Always verify cache for reloaded resources
    // if (factory->m_HttpCache)
    //     dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);

    char encoded_uri[dmResource::RESOURCE_PATH_MAX*2];
    CreateEncodedUri(&archive->m_BaseUri, path, encoded_uri, sizeof(encoded_uri));

    dmHttpClient::Result http_result = dmHttpClient::Request(archive->m_HttpClient, method, encoded_uri);

    // // Always verify cache for reloaded resources
    // if (factory->m_HttpCache)
    //     dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUSTED);

    if (http_result != dmHttpClient::RESULT_OK)
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

    bool get_file_size = strcmp(method, "HEAD") == 0;

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
        if (archive->m_HttpTotalBytesStreamed > *buffer_length)
            return dmResourceProvider::RESULT_IO_ERROR;

        *buffer_length = archive->m_HttpTotalBytesStreamed;
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

    return GetRequestFromUri((HttpProviderContext*)archive, "HEAD", path, file_size, 0);
}

static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t _buffer_len)
{
    HttpProviderContext* archive = (HttpProviderContext*)_archive;
    (void)path_hash;

    uint32_t buffer_len = _buffer_len;
    dmResourceProvider::Result result = GetRequestFromUri((HttpProviderContext*)archive, "GET", path, &buffer_len, buffer);
    if (result != dmResourceProvider::RESULT_OK)
    {
        return result;
    }
    return dmResourceProvider::RESULT_OK;
}

static void SetupArchiveLoaderHttp(dmResourceProvider::ArchiveLoader* loader)
{
    loader->m_CanMount      = MatchesUri;
    loader->m_Mount         = Mount;
    loader->m_Unmount       = Unmount;
    loader->m_GetFileSize   = GetFileSize;
    loader->m_ReadFile      = ReadFile;
}

DM_DECLARE_ARCHIVE_LOADER(ResourceProviderHttp, "http", SetupArchiveLoaderHttp);
}
