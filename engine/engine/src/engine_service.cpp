#include <stdint.h>
#include <string.h>
#include <dlib/web_server.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/ssdp.h>
#include <dlib/socket.h>
#include <dlib/sys.h>
#include <dlib/template.h>
#include <dlib/profile.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>
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
    "{\"version\": \"${ENGINE_VERSION}\"}";

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
            else
            {
                return 0;
            }
        }

        static void RedirectHandler(void*, dmWebServer::Request* request)
        {
            dmWebServer::SetStatusCode(request, 302);
            dmWebServer::SendAttribute(request, "Location", "http://localhost:8001");
        }

        bool Init(uint16_t port)
        {
            dmTemplate::Format(this, m_InfoJson, sizeof(m_InfoJson), INFO_TEMPLATE, ReplaceCallback);

            dmSys::SystemInfo info;
            dmSys::GetSystemInfo(&info);
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
                dmSocket::Result sockr = dmSocket::GetHostname(m_Name, sizeof(m_Name));
                if (sockr != dmSocket::RESULT_OK)
                {
                    return false;
                }
            }

            dmSocket::Address local_address;
            dmSocket::Result sockr = dmSocket::GetLocalAddress(&local_address);
            if (sockr != dmSocket::RESULT_OK)
            {
                return false;
            }

            dmWebServer::NewParams params;
            params.m_Port = port;
            dmWebServer::HServer web_server;
            dmWebServer::Result r = dmWebServer::New(&params, &web_server);
            if (r != dmWebServer::RESULT_OK)
            {
                dmLogError("Unable to create engine web-server (%d)", r);
                return false;
            }

            // The redirect server
            params.m_Port = 8002;
            dmWebServer::HServer web_server_redirect;
            r = dmWebServer::New(&params, &web_server_redirect);
            if (r != dmWebServer::RESULT_OK)
            {
                dmLogError("Unable to create engine (redirect) web-server (%d)", r);
                return false;
            }

            dmSocket::Address address;
            dmWebServer::GetName(web_server, &address, &m_Port);
            DM_SNPRINTF(m_PortText, sizeof(m_PortText), "%d", (int) m_Port);
            DM_SNPRINTF(m_LogPortText, sizeof(m_LogPortText), "%d", (int) dmLogGetPort());

            char* local_address_str =  dmSocket::AddressToIPString(local_address);
            dmStrlCpy(m_LocalAddress, local_address_str, sizeof(m_LocalAddress));

            // UDN must be unique and this scheme is probably unique enough
            dmStrlCpy(m_DeviceDesc.m_UDN, "defold-", sizeof(m_DeviceDesc.m_UDN));
            dmStrlCat(m_DeviceDesc.m_UDN, local_address_str, sizeof(m_DeviceDesc.m_UDN));
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

            free(local_address_str);

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

            // Redirects from old profiler to the new
            dmWebServer::HandlerParams redirect_params;
            redirect_params.m_Handler = RedirectHandler;
            redirect_params.m_Userdata = this;
            dmWebServer::AddHandler(web_server_redirect, "/", &redirect_params);

            m_WebServer = web_server;
            m_WebServerRedirect = web_server_redirect;
            m_SSDP = ssdp;
            m_Profile = 0; // Set during the update
            return true;
        }

        void Final()
        {
            dmWebServer::Delete(m_WebServer);
            dmWebServer::Delete(m_WebServerRedirect);

            if (m_SSDP)
            {
                dmSSDP::DeregisterDevice(m_SSDP, "defold");
                dmSSDP::Delete(m_SSDP);
            }
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

        dmProfile::HProfile  m_Profile;
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

    void Update(HEngineService engine_service, dmProfile::HProfile profile)
    {
        DM_PROFILE(Engine, "Service");
        engine_service->m_Profile = profile;
        dmWebServer::Update(engine_service->m_WebServer);
        dmWebServer::Update(engine_service->m_WebServerRedirect);
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

        char* service_port_env = getenv("DM_SERVICE_PORT");

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

    //
    // Resource profiler
    //

    static bool ResourceIteratorFunction(const dmResource::IteratorResource& resource, void* user_ctx)
    {
        dmWebServer::Request* request = (dmWebServer::Request*)user_ctx;

        const char* name = dmHashReverseSafe64(resource.m_Id);
        const char* extension = strrchr(name, '.');
        if (!extension)
            extension = "";

        dmWebServer::Result r;
        r = SendString(request, name); CHECK_RESULT_BOOL(r);
        r = SendString(request, extension); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &resource.m_Size, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &resource.m_SizeOnDisc, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &resource.m_RefCount, 4); CHECK_RESULT_BOOL(r);
        return true;
    }

    static void HttpResourceRequestCallback(void* context, dmWebServer::Request* request)
    {
        dmWebServer::Result r = SendString(request, FOURCC_RESOURCES);
        if (r != dmWebServer::RESULT_OK)
        {
            dmLogWarning("Unexpected http-server when transmitting profile data (%d)", r);
            return;
        }

        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");
        dmResource::HFactory factory = (dmResource::HFactory)context;
        dmResource::IterateResources(factory, ResourceIteratorFunction, (void*)request);
    }

    //
    // GameObject profiler
    //

    struct GameObjectProfilerCtx
    {
        dmWebServer::Request*   m_Request;
        // we need these variables to recreate the tree structure
        dmArray<uint32_t>       m_Stack; // A stack of indices to keep track of the traversal tree (i.e. parents)
        uint32_t                m_Index;
    };

    static bool SendGameObjectData(GameObjectProfilerCtx* ctx, dmhash_t id, dmhash_t resource_id, uint32_t index, uint32_t parent, const char* type)
    {
        // See profiler.html, loadGameObjects() for the receiving end of this code
        dmWebServer::Request* request = ctx->m_Request;
        dmWebServer::Result r;
        r = SendString(request, dmHashReverseSafe64(id)); CHECK_RESULT_BOOL(r);
        r = SendString(request, dmHashReverseSafe64(resource_id)); CHECK_RESULT_BOOL(r);
        r = SendString(request, type); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &index, 4); CHECK_RESULT_BOOL(r);
        r = dmWebServer::Send(request, &parent, 4); CHECK_RESULT_BOOL(r);
        return true;
    }

    static bool ComponentIteratorFunction(const dmGameObject::IteratorComponent* iterator, void* user_ctx)
    {
        GameObjectProfilerCtx* ctx = (GameObjectProfilerCtx*)user_ctx;
        uint32_t parent = ctx->m_Stack.Back();
        SendGameObjectData(ctx, iterator->m_NameHash, iterator->m_Resource, ctx->m_Index++, parent, iterator->m_Type);
        return true;
    }

    static bool GameObjectIteratorFunction(const dmGameObject::IteratorGameObject* iterator, void* user_ctx)
    {
        GameObjectProfilerCtx* ctx = (GameObjectProfilerCtx*)user_ctx;
        uint32_t parent = ctx->m_Stack.Back();
        uint32_t index = ctx->m_Index++;
        ctx->m_Stack.Push(index);

        SendGameObjectData(ctx, dmGameObject::GetIdentifier(iterator->m_Instance), iterator->m_Resource, index, parent, "goc");

        bool result = dmGameObject::IterateComponents(iterator->m_Instance, ComponentIteratorFunction, user_ctx);

        uint32_t lastindex = ctx->m_Stack.Back();
        ctx->m_Stack.Pop();
        assert(lastindex == index);

        return result;
    }

    static bool CollectionIteratorFunction(const dmGameObject::IteratorCollection* iterator, void* user_ctx)
    {
        GameObjectProfilerCtx* ctx = (GameObjectProfilerCtx*)user_ctx;

        uint32_t parent = ctx->m_Stack.Back();
        uint32_t index = ctx->m_Index++;
        ctx->m_Stack.Push(index);

        SendGameObjectData(ctx, iterator->m_NameHash, iterator->m_Resource, index, parent, "collectionc");

        bool result = dmGameObject::IterateGameObjects(iterator->m_Collection, GameObjectIteratorFunction, user_ctx);

        uint32_t lastindex = ctx->m_Stack.Back();
        ctx->m_Stack.Pop();
        assert(lastindex == index);

        return result;
    }

    static void HttpGameObjectRequestCallback(void* context, dmWebServer::Request* request)
    {
        dmWebServer::Result r = SendString(request, "GOBJ");
        if (r != dmWebServer::RESULT_OK)
        {
            dmLogWarning("Unexpected http-server when transmitting profile data (%d)", r);
            dmWebServer::SetStatusCode(request, 500);
            dmWebServer::Send(request, INTERNAL_SERVER_ERROR, sizeof(INTERNAL_SERVER_ERROR));
            return;
        }

        GameObjectProfilerCtx ctx;
        ctx.m_Request = request;
        ctx.m_Index = 0;
        ctx.m_Stack.SetCapacity(1024); // This is the depth of the tree. We don't expect the tree to ever be this big
        ctx.m_Stack.Push(ctx.m_Index);
        ctx.m_Index++;

        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");

        dmGameObject::HRegister regist = (dmGameObject::HRegister)context;
        bool result = dmGameObject::IterateCollections(regist, CollectionIteratorFunction, &ctx);

        uint32_t lastindex = ctx.m_Stack.Back();
        ctx.m_Stack.Pop();
        assert(lastindex == 0);

        dmWebServer::SetStatusCode(request, result ? 200 : 500);
    }

    //
    // DLIB Profiler
    //

    // Profile strings handling

    static void SendProfileString(dmWebServer::Request* request, uint64_t id, const char* str)
    {
        dmWebServer::Send(request, &id, sizeof(id));
        SendString(request, str);
    }

    static void ProfileSendScopes(void* context, const dmProfile::Scope* scope)
    {
        SendProfileString((dmWebServer::Request*)context, (uint64_t)scope, scope->m_Name);
    }

    static void ProfileSendCounters(void* context, const dmProfile::Counter* counter)
    {
        SendProfileString((dmWebServer::Request*)context, (uint64_t)counter, counter->m_Name);
    }

    static void ProfileSendStringCallback(void* context, const uintptr_t* key, const char** value)
    {
        SendProfileString((dmWebServer::Request*)context, (uint64_t)*key, *value);
    }


    // The actual payload (elapsed time, count etc)
    static void ProfileSendSamples(void* context, const dmProfile::Sample* sample)
    {
        dmWebServer::Request* request = (dmWebServer::Request*)context;
        dmWebServer::Result r;

        uint64_t name = (uint64_t)sample->m_Name;
        r = dmWebServer::Send(request, &name, 8); CHECK_RESULT(r);
        uint64_t scope = (uint64_t)sample->m_Scope;
        r = dmWebServer::Send(request, &scope, 8); CHECK_RESULT(r);

        r = dmWebServer::Send(request, &sample->m_Start, 4); CHECK_RESULT(r);
        r = dmWebServer::Send(request, &sample->m_Elapsed, 4); CHECK_RESULT(r);
        r = dmWebServer::Send(request, &sample->m_ThreadId, 2); CHECK_RESULT(r);
    }

    static void ProfileSendScopesData(void* context, const dmProfile::ScopeData* scope_data)
    {
        dmWebServer::Request* request = (dmWebServer::Request*)context;
        dmWebServer::Result r;

        uint64_t ptr = (uint64_t)scope_data->m_Scope;
        r = dmWebServer::Send(request, &ptr, 8); CHECK_RESULT(r);
        r = dmWebServer::Send(request, &scope_data->m_Elapsed, 4); CHECK_RESULT(r);
        r = dmWebServer::Send(request, &scope_data->m_Count, 4); CHECK_RESULT(r);
    }

    static void ProfileSendCountersData(void* context, const dmProfile::CounterData* counter_data)
    {
        dmWebServer::Request* request = (dmWebServer::Request*)context;
        dmWebServer::Result r;

        uint64_t ptr = (uint64_t)counter_data->m_Counter;
        r = dmWebServer::Send(request, &ptr, 8); CHECK_RESULT(r);
        r = dmWebServer::Send(request, (void*)&counter_data->m_Value, 4); CHECK_RESULT(r);
    }


    static void HttpProfileSendStrings(void* user_ctx, dmWebServer::Request* request)
    {
        HEngineService engine_service = (HEngineService)user_ctx;
        if (!engine_service->m_Profile)
        {
            dmWebServer::SetStatusCode(request, 500);
            const char* msg = "Error. The profiler was not active!";
            dmWebServer::Send(request, msg, strlen(msg));
            return;
        }

        dmProfile::Pause(true);

        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");

        dmWebServer::Result r;
        r = SendString(request, "STRS"); CHECK_RESULT(r);
        dmProfile::IterateStrings(engine_service->m_Profile, request, ProfileSendStringCallback);
        dmProfile::IterateScopes(engine_service->m_Profile, request, ProfileSendScopes);
        dmProfile::IterateCounters(engine_service->m_Profile, request, ProfileSendCounters);

        dmProfile::Pause(false);
    }

    static void HttpProfileSendFrame(void* user_ctx, dmWebServer::Request* request)
    {
        HEngineService engine_service = (HEngineService)user_ctx;
        if (!engine_service->m_Profile)
        {
            dmWebServer::SetStatusCode(request, 500);
            const char* msg = "Error. The profiler was not active!";
            dmWebServer::Send(request, msg, strlen(msg));
            return;
        }

        dmWebServer::SendAttribute(request, "Access-Control-Allow-Origin", "*");

        dmWebServer::Result r;
        r = SendString(request, "PROF"); CHECK_RESULT(r);

        const uint32_t tps = 1000000;//g_TicksPerSecond;
        r = dmWebServer::Send(request, &tps, 4); CHECK_RESULT(r);

        dmProfile::IterateSamples(engine_service->m_Profile, request, ProfileSendSamples);
        r = SendString(request, "ENDD"); CHECK_RESULT(r);

        dmProfile::IterateScopeData(engine_service->m_Profile, request, ProfileSendScopesData);
        r = SendString(request, "ENDD"); CHECK_RESULT(r);

        dmProfile::IterateCounterData(engine_service->m_Profile, request, ProfileSendCountersData);
        r = SendString(request, "ENDD"); CHECK_RESULT(r);
    }


#undef CHECK_RESULT_BOOL

    //
    // All profilers' setup
    //

    static void ProfileHandler(void* user_data, dmWebServer::Request* request)
    {
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "text/html");
        dmWebServer::Send(request, PROFILER_HTML, PROFILER_HTML_SIZE);
    }

    void InitProfiler(HEngineService engine_service, dmResource::HFactory factory, dmGameObject::HRegister regist)
    {
        dmWebServer::HandlerParams resource_params;
        resource_params.m_Handler = HttpResourceRequestCallback;
        resource_params.m_Userdata = factory;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/resources_data", &resource_params);

        dmWebServer::HandlerParams gameobject_params;
        gameobject_params.m_Handler = HttpGameObjectRequestCallback;
        gameobject_params.m_Userdata = regist;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/gameobjects_data", &gameobject_params);

        dmWebServer::HandlerParams strings_params;
        strings_params.m_Handler = HttpProfileSendStrings;
        strings_params.m_Userdata = engine_service;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/profile_strings", &strings_params);

        dmWebServer::HandlerParams frame_params;
        frame_params.m_Handler = HttpProfileSendFrame;
        frame_params.m_Userdata = engine_service;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/profile_frame", &frame_params);

        // The entry point to the engine service profiler
        dmWebServer::HandlerParams profile_params;
        profile_params.m_Handler = ProfileHandler;
        profile_params.m_Userdata = 0;
        dmWebServer::AddHandler(engine_service->m_WebServer, "/", &profile_params);
    }

}

