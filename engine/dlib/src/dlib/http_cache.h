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

#ifndef DM_HTTP_CACHE_H
#define DM_HTTP_CACHE_H

#include <stdio.h>
#include <dlib/uri.h>

namespace dmHttpCache
{
    /**
     * HTTP-cache. Represents the actual database.
     */
    typedef struct Cache* HCache;

    /**
     * HTTP-cache creator handle. Handle to create cache records.
     */
    typedef struct CacheCreator* HCacheCreator;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_ALREADY_CACHED = 1,
        RESULT_NO_ENTRY = 2,
        RESULT_LOCKED = 3,
        RESULT_INVALID_PATH = -1,
        RESULT_IO_ERROR = -2,
        RESULT_OUT_OF_RESOURCES = -3,
        RESULT_NO_ETAG = -4,
        RESULT_INVAL = -5,
    };

    /**
     * Consistency policy
     */
    enum ConsistencyPolicy
    {
        CONSISTENCY_POLICY_VERIFY = 0,     //!< Always verify etag with server
        CONSISTENCY_POLICY_TRUST_CACHE = 1,//!< Trust the local cache and avoid network round-trips
    };

    /// Maximum length of tags
    const uint32_t MAX_TAG_LEN = 64;

    // Maximum length of an URI
    const uint32_t MAX_URI_LEN = dmURI::MAX_URI_LEN;

    /**
     * Cache entry info
     */
    struct EntryInfo
    {
        /// ETag string
        char     m_ETag[MAX_TAG_LEN];
        /// Path string
        char*    m_URI;
        /// The content hash is the hash of URI and ETag.
        uint64_t m_IdentifierHash;
        /// Last accessed time
        uint64_t m_LastAccessed;
        /// Expires
        uint64_t m_Expires;
        /// Checksum
        uint64_t m_Checksum;
        uint32_t m_RangeStart;
        uint32_t m_RangeEnd;
        uint32_t m_DocumentSize;

        /// True if the entry is verified, ie the cached version is valid, during the session
        uint8_t  m_Verified : 1;
        /// Valid in terms of Cache-Control expires
        /// NOTE: For etag-entries this always set to false as max-age is implicitly set to 0 for etag-entries
        uint8_t  m_Valid : 1;
    };

    void SetDefaultParams(struct NewParams* params);

    /**
     * New parameters structure when creating a new http cache
     */
    struct NewParams
    {
        /// Path to cache directory
        const char* m_Path;

        /// Maximum age of cache entries in seconds.
        /// Default value is 60 * 60 * 24 * 5, ie 5 days
        uint64_t    m_MaxCacheEntryAge;

        NewParams()
        {
            SetDefaultParams(this);
        }
    };

    /**
     * Create or open existing http cache
     * @param params parameters
     * @param cache cachen handle, out parameter
     * @return RESULT_OK on success
     */
    Result Open(NewParams* params, HCache* cache);

    /**
     * Delete, ie close http cache and flush index to disk
     * @param cache http cache handle
     * @return RESULT_OK on success
     */
    Result Close(HCache cache);

    /**
     * Flush index to disk. Flush will only write to disk when the index is dirty.
     * @param cache http cache handle
     * @return RESULT_OK on success
     */
    Result Flush(HCache cache);

    /**
     * Begin creation of cache entry with etag and/or max-age time
     * @param cache cache
     * @param uri uri
     * @param etag etag
     * @param max_age max-age from http-header "Cache-Control"
     * @param range_start   The partial request start byte
     * @param range_end     The partial request end byte (inclusive)
     * @param document_size The full document size (!= from content length)
     * @param cache_creator cache creator handle (out)
     * @return RESULT_OK on success
     */
    Result Begin(HCache cache, const char* uri, const char* etag, uint32_t max_age, uint32_t range_start, uint32_t range_end, uint32_t document_size, HCacheCreator* cache_creator);

    /**
     * Add data to cache entry
     * @param cache cache
     * @param cache_creator cache creator handle
     * @param content content to add
     * @param content_len content length
     * @return RESULT_OK on success
     */
    Result Add(HCache cache, HCacheCreator cache_creator, const void* content, uint32_t content_len);

    /**
     * Set cache entry as erroneous
     * @param cache cache
     * @param cache_creator cache creator handle
     */
    void SetError(HCache cache, HCacheCreator cache_creator);

    /**
     * End cache entry creation
     * @param cache cache
     * @param cache_creator cache creator handle
     * @return RESULT_OK on success
     */
    Result End(HCache cache, HCacheCreator cache_creator);

    /**
     * Get current ETag of cached entry
     * @param cache http cache handle
     * @param uri uri
     * @param tag_buffer tag buffer, out parameter
     * @param tag_buffer_len size of tag buffer
     * @return RESULT_OK on success, RESULT_NO_ENTRY if the entry doesn't exist.
     */
    Result GetETag(HCache cache, const char* uri, char* tag_buffer, uint32_t tag_buffer_len);

    Result GetInfo(HCache cache, const char* uri, EntryInfo* info);

    /**
     * Get file for cache entry
     * @param cache         cache
     * @param uri           uri
     * @param etag          etag
     * @param file          file representing the cached content
     * @param file_size     size of the cached content
     * @param checksum      content checksum (dmHashString64)
     * @param range_start   start offset into original file
     * @param range_end     end offset into original file (inclusive)
     * @param document_size size of the original file
     * @return RESULT_OK on success.
     */
    Result Get(HCache cache, const char* uri, const char* etag, FILE** file, uint32_t* file_size, uint64_t* checksum,
                    uint32_t* range_start, uint32_t* range_end, uint32_t* document_size);

    /**
     * Set cache entry to verifed
     * @param cache cache
     * @param uri uri
     * @param verified true for verified.
     * @return RESULT_OK on success, RESULT_NO_ENTRY if the entry doesn't exist.
     */
    Result SetVerified(HCache cache, const char* uri, bool verified);

    /**
     * Release cache entry (for reading), see Get.
     * @param cache
     * @param uri uri
     * @param etag etag
     * @param file file representing the cached content
     * @return RESULT_OK on success.
     */
    Result Release(HCache cache, const char* uri, const char* etag, FILE* file);

    /**
     * Get total entry count in cache
     * @param cache http cache handle
     * @return entry count
     */
    uint32_t GetEntryCount(HCache cache);

    /**
     * Set consistency policy
     * @param cache cache
     * @param policy policy to set
     */
    void SetConsistencyPolicy(HCache cache, ConsistencyPolicy policy);

    /**
     * Get consistency policy
     * @param cache cache
     * @return policy
     */
    ConsistencyPolicy GetConsistencyPolicy(HCache cache);

    /**
     * Iterate over all entries
     * @param cache cache
     * @param context user context
     * @param call_back call-back
     */
    void Iterate(HCache cache, void* context, void (*call_back)(void* context, const EntryInfo* entry_info));
}

#endif
