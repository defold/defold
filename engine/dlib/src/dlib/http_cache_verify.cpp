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

#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "hash.h"
#include "hashtable.h"
#include "http_cache_verify.h"
#include "math.h"
#include "time.h"

namespace dmHttpCacheVerify
{
    struct VerifyContext
    {
        const char*                     m_CacheKeyPrefix;
        uint64_t                        m_MaxAge;
        uint64_t                        m_CurrentTime;
        dmArray<dmHttpCache::EntryInfo>  m_Entries;
        dmHashTable64<uint32_t>         m_EntryIndices;
        dmArray<char>                   m_Request;
        char                            m_Line[dmHttpCache::MAX_URI_LEN];
        uint32_t                        m_LineSize;
        bool                            m_InvalidResponse;
        int                             m_HttpStatus;

        VerifyContext(uint64_t max_age, const char* cache_key_prefix)
        : m_CacheKeyPrefix(cache_key_prefix)
        , m_MaxAge(max_age * 1000000)
        , m_CurrentTime(dmTime::GetTime())
        , m_LineSize(0)
        , m_InvalidResponse(false)
        , m_HttpStatus(0)
        {}

        ~VerifyContext()
        {
            for (uint32_t i = 0; i < m_Entries.Size(); ++i)
                free(m_Entries[i].m_URI);
        }
    };

    static void CollectEntry(void* context, const dmHttpCache::EntryInfo* entry)
    {
        VerifyContext* ctx = (VerifyContext*)context;
        if (!entry->m_ETag[0] || entry->m_LastAccessed + ctx->m_MaxAge < ctx->m_CurrentTime)
            return;
        if (ctx->m_CacheKeyPrefix &&
            (strncmp(entry->m_URI, ctx->m_CacheKeyPrefix, strlen(ctx->m_CacheKeyPrefix)) != 0 ||
             strstr(entry->m_URI, "=bytes=")))
            return;

        // Snapshot keys and tags while the cache is locked. Network I/O must
        // not hold that lock or validate entries added during the request.
        if (ctx->m_Entries.Full())
            ctx->m_Entries.OffsetCapacity(64);
        if (ctx->m_EntryIndices.Full())
            ctx->m_EntryIndices.SetCapacity(ctx->m_EntryIndices.Capacity() + 64);
        dmHttpCache::EntryInfo copy = *entry;
        copy.m_URI = strdup(entry->m_URI);
        copy.m_Verified = false;
        ctx->m_EntryIndices.Put(dmHashString64(copy.m_URI), ctx->m_Entries.Size());
        ctx->m_Entries.Push(copy);

        uint32_t uri_size = strlen(copy.m_URI);
        uint32_t tag_size = strlen(copy.m_ETag);
        uint32_t size = uri_size + tag_size + 2;
        if (ctx->m_Request.Remaining() < size)
            ctx->m_Request.OffsetCapacity(dmMath::Max(size, 16U * 1024U));
        uint32_t offset = ctx->m_Request.Size();
        ctx->m_Request.SetSize(offset + size);
        char* out = ctx->m_Request.Begin() + offset;
        memcpy(out, copy.m_URI, uri_size);
        out[uri_size] = ' ';
        memcpy(out + uri_size + 1, copy.m_ETag, tag_size);
        out[size - 1] = '\n';
    }

    static uint32_t HttpSendContentLength(dmHttpClient::HResponse, void* user_data)
    {
        return ((VerifyContext*)user_data)->m_Request.Size();
    }

    static dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* user_data)
    {
        VerifyContext* ctx = (VerifyContext*)user_data;
        return dmHttpClient::Write(response, ctx->m_Request.Begin() + offset, length);
    }

    static void FinishLine(VerifyContext* ctx)
    {
        if (ctx->m_InvalidResponse)
            return;
        if (ctx->m_LineSize && ctx->m_Line[ctx->m_LineSize - 1] == '\r')
            --ctx->m_LineSize;
        ctx->m_Line[ctx->m_LineSize] = '\0';
        uint32_t* index = ctx->m_EntryIndices.Get(dmHashString64(ctx->m_Line));
        if (index && strcmp(ctx->m_Entries[*index].m_URI, ctx->m_Line) == 0)
            ctx->m_Entries[*index].m_Verified = true;
        ctx->m_LineSize = 0;
    }

    static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t,
                            uint32_t, uint32_t, uint32_t, const char* method)
    {
        VerifyContext* ctx = (VerifyContext*)user_data;
        ctx->m_HttpStatus = status_code;
        if (!content_data && !content_data_size && !method)
        {
            ctx->m_LineSize = 0;
            ctx->m_InvalidResponse = false;
            for (uint32_t i = 0; i < ctx->m_Entries.Size(); ++i)
                ctx->m_Entries[i].m_Verified = false;
        }
        if (status_code != 200)
            return;
        const char* data = (const char*)content_data;
        for (uint32_t i = 0; i < content_data_size; ++i)
        {
            if (data[i] == '\n')
                FinishLine(ctx);
            else if (ctx->m_LineSize < sizeof(ctx->m_Line) - 1 && data[i] != '\0')
                ctx->m_Line[ctx->m_LineSize++] = data[i];
            else
                ctx->m_InvalidResponse = true;
        }
    }

    Result VerifyCache(dmHttpCache::HCache cache, dmURI::Parts* uri, uint64_t max_age,
                       const char* cache_key_prefix, FVerifiedCallback callback, void* user_data)
    {
        VerifyContext context(max_age, cache_key_prefix);
        dmHttpCache::Iterate(cache, &context, CollectEntry);
        if (context.m_Entries.Empty())
            return RESULT_OK;

        dmHttpClient::NewParams params;
        params.m_HttpSendContentLength = HttpSendContentLength;
        params.m_HttpWrite = HttpWrite;
        params.m_HttpContent = HttpContent;
        params.m_Userdata = &context;
        // Verification is an optional optimization. Fall back to ordinary
        // requests if the endpoint is unavailable or does not respond promptly.
        params.m_RequestTimeout = 5 * 1000 * 1000;
        dmHttpClient::HClient client = dmHttpClient::New(&params, uri);
        if (!client)
            return RESULT_OUT_OF_RESOURCES;
        dmHttpClient::Result result = dmHttpClient::Post(client, "/__verify_etags__");
        if (result == dmHttpClient::RESULT_OK && context.m_HttpStatus == 200 && !context.m_InvalidResponse)
        {
            // A final record need not have a trailing newline.
            FinishLine(&context);
            for (uint32_t i = 0; i < context.m_Entries.Size(); ++i)
            {
                if (context.m_Entries[i].m_Verified)
                    callback(user_data, &context.m_Entries[i]);
            }
        }
        dmHttpClient::Delete(client);
        if (result == dmHttpClient::RESULT_OK && context.m_HttpStatus == 200 && !context.m_InvalidResponse)
            return RESULT_OK;
        if (result == dmHttpClient::RESULT_NOT_200_OK && context.m_HttpStatus == 404)
            return RESULT_UNSUPPORTED;
        return result == dmHttpClient::RESULT_OK || result == dmHttpClient::RESULT_NOT_200_OK ? RESULT_UNKNOWN_ERROR : RESULT_NETWORK_ERROR;
    }

    static void SetVerified(void* cache, const dmHttpCache::EntryInfo* entry)
    {
        dmHttpCache::EntryInfo current;
        if (dmHttpCache::GetInfo((dmHttpCache::HCache)cache, entry->m_URI, &current) == dmHttpCache::RESULT_OK &&
            current.m_IdentifierHash == entry->m_IdentifierHash)
            dmHttpCache::SetVerified((dmHttpCache::HCache)cache, entry->m_URI, true);
    }

    Result VerifyCache(dmHttpCache::HCache cache, dmURI::Parts* uri, uint64_t max_age)
    {
        return VerifyCache(cache, uri, max_age, 0, SetVerified, cache);
    }
}
