#include "http_cache_verify.h"

#include "http_client.h"
#include "log.h"
#include "time.h"

namespace dmHttpCacheVerify
{
    const uint32_t BUFFER_SIZE = 512;
    struct VerifyContext
    {
        dmHttpClient::HClient   m_Client;
        dmHttpClient::HResponse m_Response;
        dmHttpCache::HCache     m_HttpCache;
        uint64_t                m_MaxAge;
        uint64_t                m_CurrentTime;
        uint32_t                m_BytesWritten;
        dmHttpClient::Result    m_Result;
        char                    m_Buffer[BUFFER_SIZE + 1]; // Room for terminating '\0'
        char*                   m_BufferCurrent;
        uint32_t                m_BufferSize;
        bool                    m_DryRun;
        int                     m_HttpStatus;

        VerifyContext(uint64_t max_age)
        {
            memset(this, 0, sizeof(*this));
            m_BufferCurrent = m_Buffer;
            m_MaxAge = max_age;
            m_CurrentTime = dmTime::GetTime();
            m_Result = dmHttpClient::RESULT_OK;
        }
    };

    static void VerifyCallback(void* context, const dmHttpCache::EntryInfo* entry_info)
    {
        VerifyContext* verify_context = (VerifyContext*) context;
        if (verify_context->m_Result != dmHttpClient::RESULT_OK)
            return;

        if (entry_info->m_LastAccessed + verify_context->m_MaxAge >= verify_context->m_CurrentTime)
        {
            // Young enough

            // #path + space + #etag + \n
            verify_context->m_BytesWritten += strlen(entry_info->m_URI) + 1 + strlen(entry_info->m_ETag) + 1;
            if (!verify_context->m_DryRun)
            {
                dmHttpClient::Result r;
                r = Write(verify_context->m_Response, entry_info->m_URI, strlen(entry_info->m_URI));
                if (r != dmHttpClient::RESULT_OK)
                {
                    verify_context->m_Result = r;
                    return;
                }

                r = Write(verify_context->m_Response, " ", 1);
                if (r != dmHttpClient::RESULT_OK)
                {
                    verify_context->m_Result = r;
                    return;
                }

                r = Write(verify_context->m_Response, entry_info->m_ETag, strlen(entry_info->m_ETag));
                if (r != dmHttpClient::RESULT_OK)
                {
                    verify_context->m_Result = r;
                    return;
                }

                r = Write(verify_context->m_Response, "\n", 1);
                if (r != dmHttpClient::RESULT_OK)
                {
                    verify_context->m_Result = r;
                    return;
                }
            }
            else
            {
                // Dry run
            }
        }
    }

    static uint32_t HttpSendContentLength(dmHttpClient::HResponse response, void* user_data)
    {
        VerifyContext* verify_context = (VerifyContext*) user_data;
        verify_context->m_DryRun = true;
        verify_context->m_Response = response;
        dmHttpCache::Iterate(verify_context->m_HttpCache, verify_context, &VerifyCallback);
        return verify_context->m_BytesWritten;
    }

    static dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, void* user_data)
    {
        VerifyContext* verify_context = (VerifyContext*) user_data;
        verify_context->m_DryRun = false;
        verify_context->m_Response = response;
        dmHttpCache::Iterate(verify_context->m_HttpCache, verify_context, &VerifyCallback);
        return verify_context->m_Result;
    }

    static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        VerifyContext* verify_context = (VerifyContext*) user_data;

        verify_context->m_HttpStatus = status_code;

        if (status_code != 200)
        {
            return;
        }

        const char* content_current = (const char*) content_data;
        const char* content_end = content_current + content_data_size;

        char* buf_current = verify_context->m_BufferCurrent;
        char* buf_end = verify_context->m_Buffer + BUFFER_SIZE;

        while (content_current < content_end)
        {
            int c = *content_current;
            if (c == '\n')
            {
                // New uri
                // Terminate string, we have room for the extra '\0'
                *buf_current = '\0';
                dmHttpCache::SetVerified(verify_context->m_HttpCache, verify_context->m_Buffer, true);

                // Reset buffer
                buf_current = verify_context->m_Buffer;
            }
            else
            {
                if (buf_current < buf_end)
                {
                    *buf_current = c;
                    buf_current++;
                }
                else
                {
                    dmLogError("Http cache verification uri entry too long");
                    // We discard the character and "wait" for '\n'
                }
            }

            ++content_current;
        }
        verify_context->m_BufferCurrent = buf_current;
    }

    Result VerifyCache(dmHttpCache::HCache cache, dmURI::Parts* uri, uint64_t max_age)
    {
        VerifyContext context(max_age * 1000000);
        context.m_HttpCache = cache;

        dmHttpClient::NewParams params;
        params.m_HttpSendContentLength = HttpSendContentLength;
        params.m_HttpWrite = HttpWrite;
        params.m_HttpContent = HttpContent;
        params.m_Userdata = &context;
        dmHttpClient::HClient client = dmHttpClient::New(&params, uri->m_Hostname, uri->m_Port);
        if (client == 0)
        {
            return RESULT_OUT_OF_RESOURCES;
        }

        context.m_Client = client;
        dmHttpClient::Result verify_result = Post(client, "/__verify_etags__");
        dmHttpClient::Delete(client);

        if (verify_result == dmHttpClient::RESULT_OK)
        {
            return RESULT_OK;
        }
        else if (verify_result == dmHttpClient::RESULT_NOT_200_OK)
        {
            if (context.m_HttpStatus == 404)
            {
                return RESULT_UNSUPPORTED;
            }
            else
            {
                return RESULT_UNKNOWN_ERROR;
            }
        }
        else
        {
            return RESULT_NETWORK_ERROR;
        }
    }

}
