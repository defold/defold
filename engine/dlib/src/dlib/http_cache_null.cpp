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

#include <string.h>

#include "http_cache.h"
#include "http_cache_verify.h"
#include "log.h"

namespace dmHttpCache
{
    void SetDefaultParams(NewParams* params)
    {
        if (params == 0)
        {
            return;
        }

        memset(params, 0, sizeof(*params));
        params->m_MaxCacheEntryAge = 60 * 60 * 24 * 5;
    }

    Result Open(NewParams* params, HCache* cache)
    {
        (void) params;

        if (cache)
        {
            *cache = 0;
        }

        dmLogError("HTTP cache is not supported on HTML5");
        return RESULT_INVAL;
    }

    Result Close(HCache cache)
    {
        (void) cache;
        return RESULT_OK;
    }

    Result Flush(HCache cache)
    {
        (void) cache;
        return RESULT_OK;
    }

    Result Begin(HCache cache, const char* uri, const char* etag, uint32_t max_age, uint32_t range_start, uint32_t range_end, uint32_t document_size, HCacheCreator* cache_creator)
    {
        (void) cache;
        (void) uri;
        (void) etag;
        (void) max_age;
        (void) range_start;
        (void) range_end;
        (void) document_size;
        if (cache_creator)
        {
            *cache_creator = 0;
        }
        return RESULT_INVAL;
    }

    Result Add(HCache cache, HCacheCreator cache_creator, const void* content, uint32_t content_len)
    {
        (void) cache;
        (void) cache_creator;
        (void) content;
        (void) content_len;
        return RESULT_INVAL;
    }

    void SetError(HCache cache, HCacheCreator cache_creator)
    {
        (void) cache;
        (void) cache_creator;
    }

    Result End(HCache cache, HCacheCreator cache_creator)
    {
        (void) cache;
        (void) cache_creator;
        return RESULT_INVAL;
    }

    Result GetETag(HCache cache, const char* uri, char* tag_buffer, uint32_t tag_buffer_len)
    {
        (void) cache;
        (void) uri;
        (void) tag_buffer;
        (void) tag_buffer_len;
        return RESULT_INVAL;
    }

    Result GetInfo(HCache cache, const char* uri, EntryInfo* info)
    {
        (void) cache;
        (void) uri;
        (void) info;
        return RESULT_INVAL;
    }

    Result Get(HCache cache, const char* uri, const char* etag, FILE** file, uint32_t* file_size, uint64_t* checksum,
                    uint32_t* range_start, uint32_t* range_end, uint32_t* document_size)
    {
        (void) cache;
        (void) uri;
        (void) etag;
        (void) file;
        (void) file_size;
        (void) checksum;
        (void) range_start;
        (void) range_end;
        (void) document_size;
        return RESULT_INVAL;
    }

    Result SetVerified(HCache cache, const char* uri, bool verified)
    {
        (void) cache;
        (void) uri;
        (void) verified;
        return RESULT_INVAL;
    }

    Result Release(HCache cache, const char* uri, const char* etag, FILE* file)
    {
        (void) cache;
        (void) uri;
        (void) etag;
        (void) file;
        return RESULT_INVAL;
    }

    uint32_t GetEntryCount(HCache cache)
    {
        (void) cache;
        return 0;
    }

    void SetConsistencyPolicy(HCache cache, ConsistencyPolicy policy)
    {
        (void) cache;
        (void) policy;
    }

    ConsistencyPolicy GetConsistencyPolicy(HCache cache)
    {
        (void) cache;
        return CONSISTENCY_POLICY_VERIFY;
    }

    void Iterate(HCache cache, void* context, void (*call_back)(void* context, const EntryInfo* entry_info))
    {
        (void) cache;
        (void) context;
        (void) call_back;
    }
}

namespace dmHttpCacheVerify
{
    Result VerifyCache(dmHttpCache::HCache cache, dmURI::Parts* uri, uint64_t max_age)
    {
        (void) cache;
        (void) uri;
        (void) max_age;
        dmLogError("HTTP cache verification is not supported on HTML5");
        return RESULT_UNSUPPORTED;
    }
}
