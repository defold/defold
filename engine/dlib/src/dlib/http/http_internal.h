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

#ifndef DM_HTTP_INTERNAL_H
#define DM_HTTP_INTERNAL_H

#include <stdint.h>

#include <dmsdk/dlib/http.h>

#ifdef __cplusplus
extern "C"
{
#endif

    HttpResult HttpNewServiceInternal(uint32_t max_concurrent_requests, HttpService** service);
    HttpResult HttpNewServiceWithCacheInternal(uint32_t max_concurrent_requests, void* http_cache, HttpService** service);
    void       HttpDeleteServiceInternal(HttpService* service);

#ifdef __cplusplus
}
#endif

#endif // DM_HTTP_INTERNAL_H
