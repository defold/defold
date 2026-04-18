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

#include <assert.h>

#include "array.h"
#include "configfile_http.h"
#include "http_client.h"
#include "math.h"

namespace dmConfigFile
{
    struct HttpContext
    {
        dmArray<char> m_Buffer;
    };

    static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        (void) response;
        (void) user_data;
        (void) status_code;
        (void) key;
        (void) value;
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                            uint32_t range_start, uint32_t range_end, uint32_t document_size,
                            const char* method)
    {
        (void) response;
        (void) content_length;
        (void) range_start;
        (void) range_end;
        (void) document_size;
        (void) method;

        HttpContext* context = (HttpContext*) user_data;
        if (status_code != 200)
            return;

        if (!content_data && !content_data_size)
        {
            context->m_Buffer.SetSize(0);
            return;
        }

        dmArray<char>& buffer = context->m_Buffer;
        if (buffer.Remaining() < content_data_size)
        {
            uint32_t new_capacity = buffer.Capacity() + dmMath::Max(4U * 1024U, content_data_size);
            context->m_Buffer.SetCapacity(new_capacity);
        }

        assert(content_data);
        buffer.PushArray((const char*) content_data, content_data_size);
    }

    Result LoadFromHttpInternal(const char* url, const dmURI::Parts& uri_parts, int argc, const char** argv, HConfig* config, FLoadFromBufferInternal load_from_buffer)
    {
        HttpContext context;

        dmHttpClient::NewParams params;
        params.m_Userdata = &context;
        params.m_HttpContent = &HttpContent;
        params.m_HttpHeader = &HttpHeader;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri_parts, 0);
        if (client == 0x0)
        {
            return RESULT_FILE_NOT_FOUND;
        }

        dmHttpClient::Result http_result = dmHttpClient::Get(client, uri_parts.m_Path);
        dmHttpClient::Delete(client);

        if (http_result != dmHttpClient::RESULT_OK)
        {
            return RESULT_FILE_NOT_FOUND;
        }

        const char* buffer = context.m_Buffer.Size() > 0 ? (const char*) &context.m_Buffer.Front() : "";
        return load_from_buffer(url, buffer, context.m_Buffer.Size(), argc, argv, config);
    }
}
