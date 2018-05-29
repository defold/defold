#ifndef DM_HTTP_CACHE_VERIFY_H
#define DM_HTTP_CACHE_VERIFY_H

#include <stdint.h>
#include <dlib/http_cache.h>
#include <dlib/uri.h>

namespace dmHttpCacheVerify
{
    /**
     * Result
     */
    enum Result
    {
        RESULT_OK = 0,               //!< RESULT_OK
        RESULT_NETWORK_ERROR = -1,   //!< RESULT_NETWORK_ERROR
        RESULT_OUT_OF_RESOURCES = -2,//!< RESULT_OUT_OF_RESOURCES
        RESULT_UNSUPPORTED = -3,     //!< RESULT_UNSUPPORTED
        RESULT_UNKNOWN_ERROR = -1000,//!< RESULT_UNKNOWN_ERROR
    };

    /**
     * Verify HTTP-cache using batched request. The server must respond a post request to /__verify_etags__
     * <pre>
     * Request format:
     * URI <SPACE> ETAG'\n'
     * </pre>
     *
     * <pre>
     * Response format:
     * URI
     * URI
     * ...
     * </pre>
     *
     * The response contains a list of verified resources.
     *
     * @param cache cache handle
     * @param uri uri
     * @param max_age max-age of resource to verify in seconds
     * @return RESULT_OK on success
     */
    Result VerifyCache(dmHttpCache::HCache cache, dmURI::Parts* uri, uint64_t max_age);

}

#endif
