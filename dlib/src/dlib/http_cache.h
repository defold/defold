#ifndef DM_HTTP_CACHE_H
#define DM_HTTP_CACHE_H

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
     * Begin creation of cache entry
     * @param cache cache
     * @param uri uri
     * @param etag etag
     * @param cache_creator cache creator handle (out)
     * @return RESULT_OK on success
     */
    Result Begin(HCache cache, const char* uri, const char* etag, HCacheCreator* cache_creator);

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

    /**
     * Get file for cache entry
     * @param cache cache
     * @param uri uri
     * @param etag etag
     * @param file file representing the cached content
     * @param checksum content checksum (dmHashString64)
     * @return RESULT_OK on success.
     */
    Result Get(HCache cache, const char* uri, const char* etag, FILE** file, uint64_t* checksum);

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
}

#endif
