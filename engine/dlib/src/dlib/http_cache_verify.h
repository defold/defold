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

#ifndef DM_HTTP_CACHE_VERIFY_H
#define DM_HTTP_CACHE_VERIFY_H

#include <dlib/http_client.h>
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
