#include <string.h>
#include <stdint.h>
#include "hash.h"
#include "log.h"
#include "hashtable.h"
#include "array.h"
#include "dstrings.h"
#include "web_server.h"
#include "http_server.h"

namespace dmWebServer
{
    struct HandlerData
    {
        void*   m_Userdata;
        Handler m_Handler;
        char    m_Prefix[64];
    };

    struct Server;

    struct InternalRequest
    {
        Server*                      m_Server;
        const dmHttpServer::Request* m_Request;
    };

    struct Server
    {
        Server()
        {
            m_StringBytesAllocated = 0;
        }

        dmHttpServer::HServer       m_HttpServer;
        dmArray<HandlerData>        m_Handlers;
        dmHashTable32<const char*>  m_Headers;
        char                        m_StringBuffer[1024];
        uint32_t                    m_StringBytesAllocated;
    };

    static void ResetHeadersTable(Server* server)
    {
        server->m_Headers.Clear();
        server->m_StringBytesAllocated = 0;
    }

    static void AddHeader(Server* server, const char* name, const char* value)
    {
        if (server->m_Headers.Full())
        {
            server->m_Headers.SetCapacity(63, server->m_Headers.Capacity() + 32);
        }

        uint32_t value_buf_length = strlen(value) + 1;
        if (sizeof(server->m_StringBuffer) - server->m_StringBytesAllocated
            >= value_buf_length)
        {
            char* p = server->m_StringBuffer + server->m_StringBytesAllocated;
            memcpy(p, value, value_buf_length);
            uint32_t name_hash = dmHashBufferNoReverse32(name, strlen(name));
            server->m_Headers.Put(name_hash, p);
            server->m_StringBytesAllocated += value_buf_length;
        }
        else
        {
            dmLogWarning("Unable to store http-header. Out of resources");
        }
    }

    void SetDefaultParams(NewParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_MaxConnections = 16;
        params->m_ConnectionTimeout = 60;
    }

    static Result TranslateResult(dmHttpServer::Result r)
    {
        switch (r)
        {
        case dmHttpServer::RESULT_OK:
            return RESULT_OK;
        case dmHttpServer::RESULT_SOCKET_ERROR:
            return RESULT_SOCKET_ERROR;
        case dmHttpServer::RESULT_INVALID_REQUEST:
            return RESULT_INVALID_REQUEST;
        case dmHttpServer::RESULT_ERROR_INVAL:
            return RESULT_ERROR_INVAL;
        case dmHttpServer::RESULT_INTERNAL_ERROR:
            return RESULT_INTERNAL_ERROR;
        case dmHttpServer::RESULT_UNKNOWN:
            return RESULT_UNKNOWN;
        }
        return RESULT_UNKNOWN;
    }

    void HttpHeader(void* user_data, const char* key, const char* value)
    {
        Server* server = (Server*) user_data;
        AddHeader(server, key, value);
    }

    void HttpResponse(void* user_data, const dmHttpServer::Request* request)
    {
        Server* server = (Server*) user_data;

        dmArray<HandlerData>& handlers = server->m_Handlers;
        uint32_t n = handlers.Size();
        HandlerData* handler = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            HandlerData* h = &handlers[i];

            if (strncmp(request->m_Resource, h->m_Prefix, strlen(h->m_Prefix)) == 0)
            {
                handler = h;
                break;
            }
        }

        if (handler)
        {
            InternalRequest internal_request;
            internal_request.m_Server = server;
            internal_request.m_Request = request;

            Request web_request;
            web_request.m_Method = request->m_Method;
            web_request.m_Resource = request->m_Resource;
            web_request.m_ContentLength = request->m_ContentLength;
            web_request.m_Internal = &internal_request;

            handler->m_Handler(handler->m_Userdata, &web_request);
        }
        else
        {
            dmHttpServer::SetStatusCode(request, 404);
            char not_found[256];
            DM_SNPRINTF(not_found, sizeof(not_found), "Resource '%s' not found", request->m_Resource);
            dmHttpServer::Send(request, not_found, strlen(not_found));
        }

        ResetHeadersTable(server);
    }

    Result New(const NewParams* params, HServer* server_out)
    {
        *server_out = 0;
        Server* server = new Server;

        dmHttpServer::NewParams http_params;
        http_params.m_Userdata = server;
        http_params.m_HttpHeader = HttpHeader;
        http_params.m_HttpResponse = HttpResponse;
        http_params.m_MaxConnections = params->m_MaxConnections;
        http_params.m_ConnectionTimeout = params->m_ConnectionTimeout;

        dmHttpServer::HServer http_server = 0;
        dmHttpServer::Result http_result;
        http_result = dmHttpServer::New(&http_params, params->m_Port, &http_server);
        if (http_result != dmHttpServer::RESULT_OK)
        {
            delete server;
            return TranslateResult(http_result);
        }

        server->m_HttpServer = http_server;
        ResetHeadersTable(server);
        *server_out = server;
        return RESULT_OK;
    }

    void Delete(HServer server)
    {
        dmHttpServer::Delete(server->m_HttpServer);
        delete server;
    }

    static HandlerData* GetHandler(HServer server, const char* prefix)
    {
        dmArray<HandlerData>& handlers = server->m_Handlers;
        uint32_t n = handlers.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            HandlerData* handler = &handlers[i];
            if (strcmp(prefix, handler->m_Prefix) == 0)
            {
                return handler;
            }
        }

        return 0;
    }

    Result AddHandler(HServer server,
                      const char* prefix,
                      const HandlerParams* handler_params)
    {
        if (GetHandler(server, prefix))
        {
            return RESULT_HANDLER_ALREADY_REGISTRED;
        }
        if (server->m_Handlers.Full())
        {
            server->m_Handlers.OffsetCapacity(16);
        }
        HandlerData handler;
        handler.m_Userdata = handler_params->m_Userdata;
        handler.m_Handler = handler_params->m_Handler;
        dmStrlCpy(handler.m_Prefix, prefix, sizeof(handler.m_Prefix));
        server->m_Handlers.Push(handler);
        return RESULT_OK;
    }

    Result RemoveHandler(HServer server, const char* prefix)
    {
        dmArray<HandlerData>& handlers = server->m_Handlers;
        uint32_t n = handlers.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            HandlerData* handler = &handlers[i];
            if (strcmp(prefix, handler->m_Prefix) == 0)
            {
                handlers.EraseSwap(i);
                return RESULT_OK;
            }
        }

        return RESULT_HANDLER_NOT_REGISTRED;
    }

    Result SetStatusCode(Request* request, int status_code)
    {
        InternalRequest* internal_request = (InternalRequest*) request->m_Internal;
        dmHttpServer::Result r = dmHttpServer::SetStatusCode(internal_request->m_Request, status_code);
        return TranslateResult(r);
    }

    const char* GetHeader(Request* request, const char* name)
    {
        uint32_t name_hash = dmHashBufferNoReverse32(name, strlen(name));
        InternalRequest* internal_request = (InternalRequest*) request->m_Internal;
        const char** value = internal_request->m_Server->m_Headers.Get(name_hash);
        if (value)
        {
            return *value;
        }
        else
        {
            return 0;
        }
    }

    Result Send(Request* request, const void* data, uint32_t data_length)
    {
        InternalRequest* internal_request = (InternalRequest*) request->m_Internal;
        dmHttpServer::Result r = dmHttpServer::Send(internal_request->m_Request, data, data_length);
        return TranslateResult(r);
    }

    Result Receive(Request* request, void* buffer, uint32_t buffer_size, uint32_t* received_bytes)
    {
        InternalRequest* internal_request = (InternalRequest*) request->m_Internal;
        dmHttpServer::Result r = dmHttpServer::Receive(internal_request->m_Request, buffer, buffer_size, received_bytes);
        return TranslateResult(r);
    }

    Result Update(HServer server)
    {
        dmHttpServer::Result r = dmHttpServer::Update(server->m_HttpServer);
        return TranslateResult(r);
    }

    void GetName(HServer server, dmSocket::Address* address, uint16_t* port)
    {
        dmHttpServer::GetName(server->m_HttpServer, address, port);
    }

    Result SendAttribute(Request* request, const char* key, const char* value)
    {
        InternalRequest* internal_request = (InternalRequest*) request->m_Internal;
        dmHttpServer::Result r = dmHttpServer::SendAttribute(internal_request->m_Request, key, value);
        return TranslateResult(r);
    }
}

