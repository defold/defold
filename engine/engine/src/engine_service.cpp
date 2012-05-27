#include <stdint.h>
#include <string.h>
#include <dlib/web_server.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <ddf/ddf.h>
#include "engine_service.h"

namespace dmEngineService
{
    struct EngineService
    {
        static void HttpServerHeader(void* user_data, const char* key, const char* value)
        {
            (void) user_data;
            (void) key;
            (void) value;
        }

        static bool ParsePostUrl(const char* resource, dmMessage::HSocket* socket, const dmDDF::Descriptor** desc, dmhash_t* message_id)
        {
            // Syntax: http://host:port/post/socket/message_type

            char buf[256];
            dmStrlCpy(buf, resource, sizeof(buf));

            char* last;
            int i = 0;
            char* s = dmStrTok(buf, "/", &last);
            bool error = false;

            while (s && !error)
            {
                switch (i)
                {
                    case 0:
                    {
                        if (strcmp(s, "post") != 0)
                        {
                            error = true;
                        }
                    }
                    break;

                    case 1:
                    {
                        dmMessage::Result mr = dmMessage::GetSocket(s, socket);
                        if (mr != dmMessage::RESULT_OK)
                        {
                            error = true;
                        }
                    }
                    break;

                    case 2:
                    {
                        *message_id = dmHashString64(s);
                        *desc = dmDDF::GetDescriptorFromHash(*message_id);
                        if (*desc == 0)
                        {
                            error = true;
                        }
                    }
                    break;
                }

                s = dmStrTok(0, "/", &last);
                ++i;
            }

            return !error;
        }

        static void SlurpHttpContent(dmWebServer::Request* request)
        {
            char buf[256];
            uint32_t total_recv = 0;

            while (total_recv < request->m_ContentLength)
            {
                uint32_t recv_bytes = 0;
                uint32_t to_read = dmMath::Min((uint32_t) sizeof(buf), request->m_ContentLength - total_recv);
                dmWebServer::Result r = dmWebServer::Receive(request, buf, to_read, &recv_bytes);
                if (r != dmWebServer::RESULT_OK)
                    return;
                total_recv += recv_bytes;
            }
        }

        static void PostHandler(void* user_data, dmWebServer::Request* request)
        {
            char msg_buf[1024];
            const char* error_msg = "";
            dmWebServer::Result r;
            uint32_t recv_bytes = 0;
            dmMessage::HSocket socket = 0;
            const dmDDF::Descriptor* desc = 0;
            dmhash_t message_id;

            if (request->m_ContentLength > sizeof(msg_buf))
            {
                error_msg = "Too large message";
                goto bail;
            }

            if (!ParsePostUrl(request->m_Resource, &socket, &desc, &message_id))
            {
                error_msg = "Invalid request";
                goto bail;
            }

            r = dmWebServer::Receive(request, msg_buf, request->m_ContentLength, &recv_bytes);
            if (r == dmWebServer::RESULT_OK)
            {
                void* msg;
                uint32_t msg_size;
                dmDDF::Result ddf_r = dmDDF::LoadMessage(msg_buf, recv_bytes, desc, &msg, dmDDF::OPTION_OFFSET_STRINGS, &msg_size);
                if (ddf_r == dmDDF::RESULT_OK)
                {
                    dmMessage::URL url;
                    url.m_Socket = socket;
                    url.m_Path = 0;
                    url.m_Fragment = 0;
                    dmMessage::Post(0, &url, message_id, 0, (uintptr_t) desc, msg, msg_size);
                    dmDDF::FreeMessage(msg);
                }
            }
            else
            {
                dmLogError("Error while reading message post data (%d)", r);
                error_msg = "Internal error";
                goto bail;
            }

            dmWebServer::SetStatusCode(request, 200);
            dmWebServer::Send(request, "OK", strlen("OK"));
            return;

    bail:
            SlurpHttpContent(request);
            dmLogError("%s", error_msg);
            dmWebServer::SetStatusCode(request, 400);
            dmWebServer::Send(request, error_msg, strlen(error_msg));
        }

        bool Init(uint16_t port)
        {
            dmWebServer::NewParams params;
            params.m_Port = port;
            dmWebServer::Result r = dmWebServer::New(&params, &m_WebServer);
            if (r == dmWebServer::RESULT_OK)
            {
                dmSocket::Address address;
                dmWebServer::GetName(m_WebServer, &address, &m_Port);

                dmWebServer::HandlerParams post_params;
                post_params.m_Handler = PostHandler;
                post_params.m_Userdata = this;
                dmWebServer::AddHandler(m_WebServer, "/post", &post_params);
                return true;
            }
            else
            {
                dmLogError("Unable to create engine web-server (%d)", r);
                return false;
            }
        }

        void Final()
        {
            dmWebServer::Delete(m_WebServer);
        }


        dmWebServer::HServer m_WebServer;
        uint16_t             m_Port;
    };

    HEngineService New(uint16_t port)
    {
        HEngineService service = new EngineService();
        if (service->Init(port))
        {
            return service;
        }
        else
        {
            delete service;
            return 0;
        }
    }

    void Delete(HEngineService engine_service)
    {
        engine_service->Final();
        delete engine_service;
    }

    void Update(HEngineService engine_service)
    {
        dmWebServer::Update(engine_service->m_WebServer);
    }

    uint16_t GetPort(HEngineService engine_service)
    {
        return engine_service->m_Port;
    }


}

