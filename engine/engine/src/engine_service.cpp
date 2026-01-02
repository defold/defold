// Copyright 2020-2025 The Defold Foundation
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

#include <stdint.h>
#include <string.h>
#include <float.h>
#include <dlib/webserver.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/ssdp.h>
#include <dlib/socket.h>
#include <dlib/sys.h>
#include <dlib/template.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>
#include <gamesys/components/comp_gui.h> 
#include "engine_service.h"
#include "engine_version.h"

extern unsigned char PROFILER_HTML[];
extern uint32_t PROFILER_HTML_SIZE;

namespace dmEngineService
{
    static const char DEVICE_DESC_TEMPLATE[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:defold=\"urn:schemas-defold-com:DEFOLD-1-0\">\n"
    "    <specVersion>\n"
    "        <major>1</major>\n"
    "        <minor>0</minor>\n"
    "    </specVersion>\n"
    "    <device>\n"
    "        <deviceType>upnp:rootdevice</deviceType>\n"
    "        <friendlyName>${NAME}</friendlyName>\n"
    "        <manufacturer>Defold</manufacturer>\n"
    "        <modelName>Defold Engine 1.0</modelName>\n"
    "        <UDN>${UDN}</UDN>\n"
    "        <defold:url>http://${HOSTNAME}:${DEFOLD_PORT}</defold:url>\n"
    "        <defold:logPort>${DEFOLD_LOG_PORT}</defold:logPort>\n"
    "    </device>\n"
    "</root>\n";

    static const char INFO_TEMPLATE[] =
    "{\"version\": \"${ENGINE_VERSION}\", \"platform\": \"${ENGINE_PLATFORM}\", \"sha1\": \"${ENGINE_SHA1}\"}";
    static const char STATE_TEMPLATE[] =
    "{\"connection_mode\": ${CONNECTION_MODE}}";

    static const char INTERNAL_SERVER_ERROR[] = "(500) Internal server error";
    const char* const FOURCC_RESOURCES = "RESS";

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
                dmDDF::Result ddf_r = dmDDF::LoadMessage(msg_buf, recv_bytes, desc, &msg, dmDDF::OPTION_OFFSET_POINTERS, &msg_size);
                if (ddf_r == dmDDF::RESULT_OK)
                {
                    dmMessage::URL url;
                    url.m_Socket = socket;
                    url.m_Path = 0;
                    url.m_Fragment = 0;
                    dmMessage::Post(0, &url, message_id, 0, (uintptr_t) desc, msg, msg_size, 0);
                    dmDDF::FreeMessage(msg);
                }
            }
            else
            {
                dmLogError("Error while reading message post data (%d)", r);
                error_msg = INTERNAL_SERVER_ERROR;
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

        static void PingHandler(void* user_data, dmWebServer::Request* request)
        {
            dmWebServer::SetStatusCode(request, 200);
            dmWebServer::Send(request, "PONG\n", strlen("PONG\n"));
        }

        static void InfoHandler(void* user_data, dmWebServer::Request* request)
        {
            EngineService* service = (EngineService*) user_data;
            dmWebServer::SetStatusCode(request, 200);
            dmWebServer::Send(request, service->m_InfoJson, strlen(service->m_InfoJson));
        }

        static void StateHandler(void* user_data, dmWebServer::Request* request)
        {
            EngineService* service = (EngineService*) user_data;
            dmWebServer::SetStatusCode(request, 200);
            dmWebServer::Send(request, service->m_StateJson, strlen(service->m_StateJson));
        }
        

        // This is equivalent to what SSDP is doing when serving the UPNP descriptor through its own http server
        // See ssdp.cpp#ReplaceHttpHostVar
        static const char* ReplaceHttpHostVar(void *user_data, const char *key)
        {
            if (strcmp(key, "HTTP-HOST") == 0)
            {
                return (char*) user_data;
            }
            return 0;
        }

        // See comment below where this handler is registered
        static void UpnpHandler(void* user_data, dmWebServer::Request* request)
        {
            EngineService* service = (EngineService*) user_data;
            char host_buffer[64];
            const char* host_header = dmWebServer::GetHeader(request, "Host");
            if (host_header == 0)
            {
                host_header = dmWebServer::GetHeader(request, "host");
            }
            if (host_header == 0)
            {
                *host_buffer = 0;
            }
            else
            {
                dmStrlCpy(host_buffer, host_header, sizeof(host_buffer));
            }
            // Strip possible port number included here
            char *delim = strchr(host_buffer, ':');
            if (delim)
            {
                *delim = 0;
            }
            char buffer[1024];
            dmTemplate::Result tr = dmTemplate::Format(host_buffer, buffer, sizeof(buffer), service->m_DeviceDesc.m_DeviceDescription, ReplaceHttpHostVar);
            if (tr != dmTemplate::RESULT_OK)
            {
                dmLogError("Error formating http response (%d)", tr);
                dmWebServer::SetStatusCode(request, 500);
                dmWebServer::Send(request, INTERNAL_SERVER_ERROR, sizeof(INTERNAL_SERVER_ERROR));
            }
            else
            {
                dmWebServer::SetStatusCode(request, 200);
                dmWebServer::Send(request, buffer, strlen(buffer));
            }
        }

        static const char* ReplaceCallback(void* user_data, const char* key)
        {
            EngineService* self = (EngineService*) user_data;
            if (strcmp(key, "UDN") == 0)
            {
                return self->m_DeviceDesc.m_UDN;
            }
            else if (strcmp(key, "DEFOLD_PORT") == 0)
            {
                return self->m_PortText;
            }
            else if (strcmp(key, "DEFOLD_LOG_PORT") == 0)
            {
                return self->m_LogPortText;
            }
            else if (strcmp(key, "NAME") == 0)
            {
                return self->m_Name;
            }
            else if (strcmp(key, "HOSTNAME") == 0)
            {
                // this will be filled in by the SSDP response
                return "${HTTP-HOST}";
            }
            else if (strcmp(key, "ENGINE_VERSION") == 0)
            {
                return dmEngineVersion::VERSION;
            }
            else if (strcmp(key, "ENGINE_SHA1") == 0)
            {
                return dmEngineVersion::VERSION_SHA1;
            }
            else if (strcmp(key, "ENGINE_PLATFORM") == 0)
            {
                return dmEngineVersion::PLATFORM;
            }
            else if (strcmp(key, "CONNECTION_MODE") == 0)
            {
                return ((EngineState*)user_data)->m_ConnectionAppMode ? "true" : "false";
            }
            else
            {
                return 0;
            }
        }

        static void RedirectHandler(void* ctx, dmWebServer::Request* request)
        {
            HEngineService engine_service = (HEngineService)ctx;
            char redirect[256];
            dmSnPrintf(redirect, sizeof(redirect), "http://%s:%d%s", engine_service->m_LocalAddress, engine_service->m_Port, request->m_Resource);
            dmWebServer::SetStatusCode(request, 302);
            dmWebServer::SendAttribute(request, "Location", redirect);
            dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        }

        bool Init(uint16_t port)
        {
            dmTemplate::Format(this, m_InfoJson, sizeof(m_InfoJson), INFO_TEMPLATE, ReplaceCallback);

            dmSys::SystemInfo info;
            dmSys::GetSystemInfo(&info);

            dmSocket::Address local_address;
            dmSocket::Result sockr = dmSocket::GetLocalAddress(&local_address);
            if (sockr != dmSocket::RESULT_OK)
            {
                return false;
            }

            m_Name[0] = 0;

            /*
             * NOTE: On Android localhost is returned for dmSocket::GetHostname.
             * Therefore we use MANUFACTURER-DEVICEMODEL instead for display-name
             *
             */
            if (strcmp(info.m_SystemName, "Android") == 0) {
                dmStrlCpy(m_Name, info.m_Manufacturer, sizeof(m_Name));
                dmStrlCat(m_Name, "-", sizeof(m_Name));
                dmStrlCat(m_Name, info.m_DeviceModel, sizeof(m_Name));
            } else {
                dmSocket::GetHostname(m_Name, sizeof(m_Name));
            }

            const char* addr = dmSocket::AddressToIPString(local_address);
            if (strstr(m_Name, addr) == 0)
            {
                dmStrlCat(m_Name, " - ", sizeof(m_Name));
                dmStrlCat(m_Name, addr, sizeof(m_Name));
            }
            free((void*)addr);

            dmStrlCat(m_Name, " - ", sizeof(m_Name));
            dmStrlCat(m_Name, info.m_SystemName, sizeof(m_Name));

            dmWebServer::NewParams params;
            params.m_Port = port;
            dmWebServer::HServer web_server;
            dmWebServer::Result r = dmWebServer::New(&params, &web_server);
            if (r != dmWebServer::RESULT_OK)
            {
                dmLogError("Unable to create engine web-server (%d)", r);
                return false;
            }

            dmSocket::Address address;
            dmWebServer::GetName(web_server, &address, &m_Port);
            dmSnPrintf(m_PortText, sizeof(m_PortText), "%d", (int) m_Port);
            dmSnPrintf(m_LogPortText, sizeof(m_LogPortText), "%d", (int) dmLog::GetPort());

            // The redirect server
            params.m_Port = 8002;
            dmWebServer::HServer web_server_redirect = 0;
            r = dmWebServer::New(&params, &web_server_redirect);
            if (r != dmWebServer::RESULT_OK)
            {
                dmLogWarning("Unable to create engine (redirect) web-server (%d), use port %d for engine services instead", r, m_Port);
            }

            // Our profiler doesn't support Ipv6 addresses, so let's assume localhost if it is Ipv6
            if (local_address.m_family == dmSocket::DOMAIN_IPV4)
            {
                char* local_address_str = dmSocket::AddressToIPString(local_address);
                dmStrlCpy(m_LocalAddress, local_address_str, sizeof(m_LocalAddress));
                free(local_address_str);
            }
            else
            {
                dmStrlCpy(m_LocalAddress, "localhost", sizeof(m_LocalAddress));
            }

            // UDN must be unique and this scheme is probably unique enough
            dmStrlCpy(m_DeviceDesc.m_UDN, "defold-", sizeof(m_DeviceDesc.m_UDN));
            dmStrlCat(m_DeviceDesc.m_UDN, m_LocalAddress, sizeof(m_DeviceDesc.m_UDN));
            dmStrlCat(m_DeviceDesc.m_UDN, ":", sizeof(m_DeviceDesc.m_UDN));
            /*
             * Note that we use the engine service port for
             * distinguishing the dmengine instances rather than the
             * ssdp http server port. Several dmengine's all running
             * on the standard port (8001) will thus be
             * indistinguishable. Having them show up as separate
             * devices is pointless since we can't determine which one
             * we're connecting to anyhow (port reuse).
             */
            dmStrlCat(m_DeviceDesc.m_UDN, m_PortText, sizeof(m_DeviceDesc.m_UDN));
            dmStrlCat(m_DeviceDesc.m_UDN, "-", sizeof(m_DeviceDesc.m_UDN));
            dmStrlCat(m_DeviceDesc.m_UDN, info.m_DeviceModel, sizeof(m_DeviceDesc.m_UDN));

            dmTemplate::Format(this, m_DeviceDescXml, sizeof(m_DeviceDescXml), DEVICE_DESC_TEMPLATE, ReplaceCallback);

            m_DeviceDesc.m_Id = "defold";
            m_DeviceDesc.m_DeviceType = "upnp:rootdevice";
            m_DeviceDesc.m_DeviceDescription = m_DeviceDescXml;

            dmSSDP::NewParams ssdp_params;
            ssdp_params.m_MaxAge = 60;
            ssdp_params.m_AnnounceInterval = 30;
            dmSSDP::HSSDP ssdp;
            dmSSDP::Result sr = dmSSDP::New(&ssdp_params, &ssdp);
            if (sr == dmSSDP::RESULT_OK)
            {
                sr = dmSSDP::RegisterDevice(ssdp, &m_DeviceDesc);
                if (sr != dmSSDP::RESULT_OK)
                {
                    dmSSDP::Delete(ssdp);
                    ssdp = 0;
                    dmLogWarning("Unable to register ssdp device (%d)", sr);
                }
            }
            else
            {
                ssdp = 0;
                dmLogWarning("Unable to create ssdp service (%d)", sr);
            }

            dmWebServer::HandlerParams post_params;
            post_params.m_Handler = PostHandler;
            post_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server, "/post", &post_params);

            dmWebServer::HandlerParams ping_params;
            ping_params.m_Handler = PingHandler;
            ping_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server, "/ping", &ping_params);

            dmWebServer::HandlerParams info_params;
            info_params.m_Handler = InfoHandler;
            info_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server, "/info", &info_params);

            // The purpose of this handler is both for debugging but also for Editor2,
            // where the user can manually specify an IP (and optionally port) to connect to.
            // The port is known (8001) or set via environment variable DM_SERVICE_PORT and logged on startup.
            dmWebServer::HandlerParams upnp_params;
            upnp_params.m_Handler = UpnpHandler;
            upnp_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server, "/upnp", &upnp_params);

            dmWebServer::HandlerParams state_params;
            state_params.m_Handler = StateHandler;
            state_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server, "/state", &state_params);

            // Redirects from old profiler to the new
            if (web_server_redirect)
            {
                dmWebServer::HandlerParams redirect_params;
                redirect_params.m_Handler = RedirectHandler;
                redirect_params.m_Userdata = this;
                dmWebServer::AddHandler(web_server_redirect, "/", &redirect_params);
            }

            m_WebServer = web_server;
            m_WebServerRedirect = web_server_redirect;
            m_SSDP = ssdp;
            m_Profile = 0; // Set during the update

            dmLogInfo("Target listening with name: %s", m_Name);

            return true;
        }

        void Final()
        {
            dmWebServer::Delete(m_WebServer);

            if (m_WebServerRedirect)
            {
                dmWebServer::Delete(m_WebServerRedirect);
            }

            if (m_SSDP)
            {
                dmSSDP::DeregisterDevice(m_SSDP, "defold");
                dmSSDP::Delete(m_SSDP);
            }
        }

        void FillState(EngineState* state)
        {
            dmTemplate::Format(state, m_StateJson, sizeof(m_StateJson), STATE_TEMPLATE, ReplaceCallback);
        }

        dmWebServer::HServer m_WebServer;
        dmWebServer::HServer m_WebServerRedirect; // A redirect from 8002 to the engine service
        uint16_t             m_Port;
        char                 m_PortText[16];
        char                 m_LogPortText[16];
        char                 m_Name[128];
        char                 m_LocalAddress[128];

        dmSSDP::DeviceDesc   m_DeviceDesc;
        char                 m_DeviceDescXml[sizeof(DEVICE_DESC_TEMPLATE) + 512]; // 512 is rather arbitrary :-)
        dmSSDP::HSSDP        m_SSDP;

        char                 m_InfoJson[sizeof(INFO_TEMPLATE) + 512]; // 512 is rather arbitrary :-)
        char                 m_StateJson[sizeof(STATE_TEMPLATE) + 512]; // 512 is rather arbitrary :-)

        ResourceHandlerParams m_ResourceHandlerParams;
        HProfile             m_Profile;
    };

    HEngineService New(uint16_t port)
    {
        HEngineService service = new EngineService();
        if (service->Init(port))
        {
            /*
             * This message is parsed by editor 2 - don't remove or change without
             * corresponding changes in engine.clj
             */
            dmLogInfo("Engine service started on port %u", (unsigned int) GetPort(service));
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

    void Update(HEngineService engine_service, HProfile profile)
    {
        DM_PROFILE("Service");
        engine_service->m_Profile = profile;
        dmWebServer::Update(engine_service->m_WebServer);
        if (engine_service->m_WebServerRedirect)
        {
            dmWebServer::Update(engine_service->m_WebServerRedirect);
        }

        engine_service->m_Profile = 0; // Don't leave a dangling pointer

        if (engine_service->m_SSDP)
        {
            dmSSDP::Update(engine_service->m_SSDP, false);
        }
    }

    uint16_t GetPort(HEngineService engine_service)
    {
        return engine_service->m_Port;
    }

    uint16_t GetServicePort(uint16_t default_port)
    {
        uint16_t engine_port = default_port;

        char* service_port_env = dmSys::GetEnv("DM_SERVICE_PORT");

        // editor 2 specifies DM_SERVICE_PORT=dynamic when launching dmengine
        if (service_port_env) {
            unsigned int env_port = 0;
            if (sscanf(service_port_env, "%u", &env_port) == 1) {
                engine_port = (uint16_t) env_port;
            }
            else if (strcmp(service_port_env, "dynamic") == 0) {
                engine_port = 0;
            }
        }

        return engine_port;
    }

    dmWebServer::HServer GetWebServer(HEngineService engine_service)
    {
        return engine_service ? engine_service->m_WebServer : 0;
    }

    #define CHECK_RESULT_BOOL(_RESULT) \
        if (r != dmWebServer::RESULT_OK)\
        {\
            dmLogWarning("Unexpected http-server when transmitting profile data (%d)", _RESULT); \
            return false; \
        }

    #define CHECK_RESULT(_RESULT) \
        if (r != dmWebServer::RESULT_OK)\
        {\
            dmLogWarning("Unexpected http-server when transmitting profile data (%d)", _RESULT); \
        }

    static dmWebServer::Result SendString(dmWebServer::Request* request, const char* str)
    {
        uint16_t len = (uint16_t)strlen(str);
        dmWebServer::Result r = dmWebServer::Send(request, &len, 2);
        if (r != dmWebServer::RESULT_OK)
        {
            return r;
        }
        r = dmWebServer::Send(request, str, len);
        return r;
    }

    static dmWebServer::Result SendText(dmWebServer::Request* request, const char* str)
    {
        return dmWebServer::Send(request, str, strlen(str));
    }


    //
    // Resource profiler
    //

    static bool SendResourceData(dmWebServer::Request* request, const char* name, const char* extension, uint32_t size, uint32_t sizeOnDisc, uint32_t refCount)
    {
        dmWebServer::Result r;
        r = SendString(request, name); CHECK_RESULT_BOOL(r);
        r = SendString(request, extension); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &size, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &sizeOnDisc, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &refCount, 4); CHECK_RESULT_BOOL(r);
        return true;
    }

    static bool ResourceIteratorFunction(const dmResource::IteratorResource& resource, void* user_ctx)
    {
        dmWebServer::Request* request = (dmWebServer::Request*)user_ctx;

        const char* name = dmHashReverseSafe64(resource.m_Id);
        const char* extension = strrchr(name, '.');
        if (!extension)
            extension = "";
        return SendResourceData(request, name, extension, resource.m_Size, resource.m_SizeOnDisc, resource.m_RefCount);
    }

    static void OutputGuiDynamicTextures(dmGameObject::SceneNode* node, dmWebServer::Request* request)
    {
        static const dmhash_t s_PropertyResource = dmHashString64("resource");
        static const dmhash_t s_PropertyType = dmHashString64("type");

        if (node->m_Type == dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT)
            return;

        dmhash_t resource_id = 0;
        dmhash_t type = 0;
        dmGameObject::SceneNodePropertyIterator pit = TraverseIterateProperties(node);
        while(dmGameObject::TraverseIteratePropertiesNext(&pit))
        {
           if (pit.m_Property.m_NameHash == s_PropertyResource)
                resource_id = pit.m_Property.m_Value.m_Hash;
            else if (pit.m_Property.m_NameHash == s_PropertyType)
                type = pit.m_Property.m_Value.m_Hash;
        }

        dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(node);
        while(dmGameObject::TraverseIterateNext(&it))
        {
            OutputGuiDynamicTextures( &it.m_Node, request );
        }
    }

    static void HttpResourceRequestCallback(void* context, dmWebServer::Request* request)
    {
        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");

        dmWebServer::Result r = SendString(request, FOURCC_RESOURCES);
        if (r != dmWebServer::RESULT_OK)
        {
            dmLogWarning("Unexpected http-server when transmitting profile data (%d)", r);
            return;
        }
        ResourceHandlerParams* params = (ResourceHandlerParams*)context;
        dmResource::IterateResources(params->m_Factory, ResourceIteratorFunction, (void*)request);

        // Collect dynamic textures from gui
        dmGameObject::SceneNode root;
        if (!dmGameObject::TraverseGetRoot(params->m_Regist, &root))
        {
            return;
        }
        OutputGuiDynamicTextures(&root, request);
    }

    //
    // GameObject profiler
    //

    static bool SendGameObjectData(dmWebServer::Request* request, dmhash_t id, dmhash_t resource_id, dmhash_t type, uint32_t index, uint32_t parent)
    {
        // See profiler.html, loadGameObjects() for the receiving end of this code
        dmWebServer::Result r;
        r = SendString(request, dmHashReverseSafe64(id)); CHECK_RESULT_BOOL(r);
        r = SendString(request, dmHashReverseSafe64(resource_id)); CHECK_RESULT_BOOL(r);
        r = SendString(request, dmHashReverseSafe64(type)); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &index, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &parent, 4); CHECK_RESULT_BOOL(r);
        return true;
    }

    static void OutputResourceSceneGraph(dmGameObject::SceneNode* node, uint32_t parent, uint32_t* counter, dmWebServer::Request* request)
    {
        static const dmhash_t s_PropertyId = dmHashString64("id");
        static const dmhash_t s_PropertyResource = dmHashString64("resource");
        static const dmhash_t s_PropertyType = dmHashString64("type");

        if (node->m_Type == dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT)
            return;

        dmhash_t id = 0;
        dmhash_t resource_id = 0;
        dmhash_t type = 0;
        dmGameObject::SceneNodePropertyIterator pit = TraverseIterateProperties(node);
        while(dmGameObject::TraverseIteratePropertiesNext(&pit))
        {
            if (pit.m_Property.m_NameHash == s_PropertyId)
                id = pit.m_Property.m_Value.m_Hash;
            else if (pit.m_Property.m_NameHash == s_PropertyResource)
                resource_id = pit.m_Property.m_Value.m_Hash;
            else if (pit.m_Property.m_NameHash == s_PropertyType)
                type = pit.m_Property.m_Value.m_Hash;
        }

        uint32_t index = (*counter)++;

        SendGameObjectData(request, id, resource_id, type, index, parent);

        dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(node);
        while(dmGameObject::TraverseIterateNext(&it))
        {
            OutputResourceSceneGraph( &it.m_Node, index, counter, request );
        }
    }

    static void HttpGameObjectRequestCallback(void* context, dmWebServer::Request* request)
    {
        dmGameObject::HRegister regist = (dmGameObject::HRegister)context;

        dmGameObject::SceneNode root;
        if (!dmGameObject::TraverseGetRoot(regist, &root))
        {
            dmWebServer::SetStatusCode(request, 500);
            SendText(request, "Failed to get root node");
            return;
        }

        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");

        SendString(request, "GOBJ");

        uint32_t counter = 1;
        OutputResourceSceneGraph(&root, 0, &counter, request);
    }

    static void SendIndent(dmWebServer::Request* request, int indent)
    {
        const char buf[4] = {' ', ' ', ' ', ' '};
        for (int i = 0; i < indent; ++i)
            dmWebServer::Send(request, buf, sizeof(buf));
    }

    static void SendJsonEscapedText(dmWebServer::Request* request, const char* text)
    {
        SendText(request, "\"");
        for (const char* p = text; *p; ++p)
        {
            switch (*p)
            {
                case '\"': SendText(request, "\\\""); break;
                case '\\': SendText(request, "\\\\"); break;
                case '\n': SendText(request, "\\n"); break;
                case '\r': SendText(request, "\\r"); break;
                case '\t': SendText(request, "\\t"); break;
                default:
                {
                    char buf[2] = {*p, 0};
                    SendText(request, buf);
                    break;
                }
            }
        }
        SendText(request, "\"");
    }

    static float JsonSafeFloat(float f)
    {
        return isfinite(f) ? f : FLT_MAX;
    }

    static void OutputJsonProperty(dmGameObject::SceneNodeProperty* property, dmWebServer::Request* request, int indent)
    {
        SendIndent(request, indent);
        SendText(request, "\"");
        SendText(request, dmHashReverseSafe64(property->m_NameHash));
        SendText(request, "\": ");

        char buffer[512];
        buffer[0] = 0;

        switch(property->m_Type)
        {
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH:
            {
                const char* rev_hash = (const char*)dmHashReverse64(property->m_Value.m_Hash, 0);
                if (rev_hash)
                    dmSnPrintf(buffer, sizeof(buffer), "\"%s\"", rev_hash);
                else
                    dmSnPrintf(buffer, sizeof(buffer), "\"0x%016llX\"", (unsigned long long)property->m_Value.m_Hash);
            }
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_NUMBER: dmSnPrintf(buffer, sizeof(buffer), "%f", JsonSafeFloat(property->m_Value.m_Number)); break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN: dmSnPrintf(buffer, sizeof(buffer), "%d", property->m_Value.m_Bool?1:0); break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3: dmSnPrintf(buffer, sizeof(buffer), "[%f, %f, %f]",
            JsonSafeFloat(property->m_Value.m_V4[0]),
            JsonSafeFloat(property->m_Value.m_V4[1]),
            JsonSafeFloat(property->m_Value.m_V4[2]));
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4:
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_QUAT: dmSnPrintf(buffer, sizeof(buffer), "[%f, %f, %f, %f]",
            JsonSafeFloat(property->m_Value.m_V4[0]),
            JsonSafeFloat(property->m_Value.m_V4[1]),
            JsonSafeFloat(property->m_Value.m_V4[2]),
            JsonSafeFloat(property->m_Value.m_V4[3]));
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_URL: dmSnPrintf(buffer, sizeof(buffer), "\"%s\"", property->m_Value.m_URL); break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_TEXT: SendJsonEscapedText(request, property->m_Value.m_Text); break;
        default: break;
        }

        if (buffer[0] != 0)
        {
            SendText(request, buffer);
        }
    }

    static void OutputJsonSceneGraph(dmGameObject::SceneNode* node, dmWebServer::Request* request, int indent)
    {
        SendIndent(request, indent);
        SendText(request, "{\n");

        bool first_property = true;
        dmGameObject::SceneNodePropertyIterator pit = TraverseIterateProperties(node);
        while(dmGameObject::TraverseIteratePropertiesNext(&pit))
        {
            if (!first_property)
                SendText(request, ",\n");
            first_property = false;

            OutputJsonProperty( &pit.m_Property, request, indent+1 );
        }

        if (!first_property)
            SendText(request, ",\n");

        SendIndent(request, indent+1);
        SendText(request, "\"children\": [");

        bool first_object = true;
        dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(node);
        while(dmGameObject::TraverseIterateNext(&it))
        {
            if (!first_object)
                SendText(request, ",\n");
            else
                SendText(request, "\n");
            first_object = false;

            OutputJsonSceneGraph( &it.m_Node, request, indent+1 );
        }
        SendText(request, "]\n");

        SendIndent(request, indent);
        SendText(request, "}");
    }

    static void HttpSceneGraphRequestCallback(void* context, dmWebServer::Request* request)
    {
        dmGameObject::HRegister regist = (dmGameObject::HRegister)context;

        dmGameObject::SceneNode root;
        if (!dmGameObject::TraverseGetRoot(regist, &root))
        {
            dmWebServer::SetStatusCode(request, 500);
            SendText(request, "Failed to get root node");
            return;
        }

        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");

        OutputJsonSceneGraph(&root, request, 0);
    }

#undef CHECK_RESULT_BOOL

    //
    // All profilers' setup
    //

    static void ProfileHandler(void* user_data, dmWebServer::Request* request)
    {
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "text/html");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        dmWebServer::Send(request, PROFILER_HTML, PROFILER_HTML_SIZE);
    }

    void InitProfiler(HEngineService engine_service, dmResource::HFactory factory, dmGameObject::HRegister regist)
    {
        dmWebServer::HandlerParams resource_params;
        resource_params.m_Handler = HttpResourceRequestCallback;
        engine_service->m_ResourceHandlerParams.m_Factory = factory;
        engine_service->m_ResourceHandlerParams.m_Regist = regist;
        resource_params.m_Userdata = &engine_service->m_ResourceHandlerParams;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/resources_data", &resource_params);

        dmWebServer::HandlerParams gameobject_params;
        gameobject_params.m_Handler = HttpGameObjectRequestCallback;
        gameobject_params.m_Userdata = regist;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/gameobjects_data", &gameobject_params);

        dmWebServer::HandlerParams scenegraph_params;
        scenegraph_params.m_Handler = HttpSceneGraphRequestCallback;
        scenegraph_params.m_Userdata = regist;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/scene_graph", &scenegraph_params);

        // The entry point to the engine service profiler
        dmWebServer::HandlerParams profile_params;
        profile_params.m_Handler = ProfileHandler;
        profile_params.m_Userdata = 0;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/", &profile_params);
    }

     void InitState(HEngineService engine_service, EngineState* state)
     {
        engine_service->FillState(state);
     }
}
