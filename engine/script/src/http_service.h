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

#ifndef DM_HTTP_SERVICE_H
#define DM_HTTP_SERVICE_H

#include <stdint.h>
#include <dlib/http_cache.h>

namespace dmHttpService
{
    typedef struct HttpService* HHttpService;

    typedef void (*ReportProgressCallback)(dmHttpDDF::HttpRequestProgress* msg, dmMessage::URL* url, uintptr_t user_data);

    struct Params
    {
    	Params()
        : m_HttpCache(0)
        , m_ReportProgressCallback(0)
        , m_ThreadCount(4)
    	{}

        dmHttpCache::HCache    m_HttpCache;
        ReportProgressCallback m_ReportProgressCallback;
    	uint32_t               m_ThreadCount  : 4;
    };

    HHttpService New(const Params* params);
    dmMessage::HSocket GetSocket(HHttpService http_service);
    void Delete(HHttpService http_service);

}  // namespace dmHttpService


#endif // #ifndef DM_HTTP_SERVICE_H
