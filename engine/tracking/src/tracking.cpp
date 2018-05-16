#include <script/script.h>
#include <dlib/log.h>

#include "tracking.h"
#include "../proto/tracking/tracking_ddf.h"

extern "C"
{
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

extern unsigned char TRACKING_LUA[];
extern uint32_t TRACKING_LUA_SIZE;

namespace dmTracking
{
    const char* TRACKING_SOCKET_NAME = "@tracking";
    const char* TRACKING_SCRIPT = "TrackingScript";

    struct Context
    {
        dmScript::HContext m_ScriptCtx;
        dmMessage::HSocket m_Socket;
        int m_ContextReference;
        int m_ContextTableReference;
    };

    struct Script
    {
        HContext m_Context;
    };

    static int TrackingScriptGetURL(lua_State* L)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(url);

        dmScript::GetInstance(L);
        Script* script = 0x0;
        if (dmScript::IsUserType(L, -1, TRACKING_SCRIPT)) {
            script = (Script*)lua_touserdata(L, -1);
            url.m_Socket = script->m_Context->m_Socket;
        }

        dmScript::PushURL(L, url);
        return 1;
    }

    static int TrackingGetInstanceContextTableRef(lua_State* L)
    {
        dmScript::GetInstance(L);

        const int self_index = 1;

        Script* script = (Script*)lua_touserdata(L, self_index);
        lua_pop(L, 1);
        lua_pushnumber(L, script && script->m_Context ? script->m_Context->m_ContextTableReference : LUA_NOREF);
        return 1;
    }

    static const luaL_reg TrackingScript_methods[] =
    {
        {0,0}
    };

    static const luaL_reg TrackingScript_meta[] =
    {
        {dmScript::META_TABLE_GET_URL,                  TrackingScriptGetURL},
        {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, TrackingGetInstanceContextTableRef},
        {0, 0}
    };

    HContext New(dmConfigFile::HConfig config_file)
    {
        Context* ctx = new Context;

        dmMessage::Result result = dmMessage::NewSocket(TRACKING_SOCKET_NAME, &ctx->m_Socket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogFatal("Could not create socket '%s'.", TRACKING_SOCKET_NAME);
            delete ctx;
            return 0;
        }

        ctx->m_ScriptCtx = dmScript::NewContext(config_file, 0, false);
        dmScript::Initialize(ctx->m_ScriptCtx);

        lua_State* L = dmScript::GetLuaState(ctx->m_ScriptCtx);

        dmScript::RegisterUserType(L, TRACKING_SCRIPT, TrackingScript_methods, TrackingScript_meta);

        Script* script = (Script*)lua_newuserdata(L, sizeof(Script));
        script->m_Context = ctx;
        luaL_getmetatable(L, TRACKING_SCRIPT);
        lua_setmetatable(L, -2);
        ctx->m_ContextReference = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_newtable(L);
        ctx->m_ContextTableReference = dmScript::Ref(L, LUA_REGISTRYINDEX);

        int top = lua_gettop(L);
        if (luaL_loadbuffer(L, (const char*)TRACKING_LUA, TRACKING_LUA_SIZE, "tracking.lua") != 0)
        {
            dmLogError("%s", lua_tolstring(L, -1, 0));
        }
        else
        {
            int ret = dmScript::PCall(L, 0, 0);
            if (ret != 0)
            {
                assert(top == lua_gettop(L));
            }
        }

        lua_pop(L, lua_gettop(L) - top);


        return ctx;
    }

    dmMessage::HSocket GetSocket(HContext context)
    {
        return context->m_Socket;
    }

    void Start(HContext context, const char* saves_app_name, const char *defold_version)
    {
        lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
        const int top = lua_gettop(L);

        lua_getglobal(L, "start");
        lua_pushstring(L, saves_app_name);
        lua_pushstring(L, defold_version);
        int ret = dmScript::PCall(L, 2, 0);
        if (ret != 0)
        {
            dmLogError("Could not start stats system.");
        }
        assert(top == lua_gettop(L));
    }

    void Finalize(HContext context)
    {
        // Update just to get the dispatch called.
        Update(context, 0.0f);

        lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
        lua_getglobal(L, "finalize");
        dmScript::PCall(L, 0, 0);
    }

    void Delete(HContext context)
    {
        lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
        dmScript::Finalize(context->m_ScriptCtx);
        dmScript::Unref(L, LUA_REGISTRYINDEX, context->m_ContextTableReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, context->m_ContextReference);
        dmScript::DeleteContext(context->m_ScriptCtx);
        dmMessage::Consume(context->m_Socket);
        dmMessage::DeleteSocket(context->m_Socket);
        delete context;
    }

    static void DispatchCallback(dmMessage::Message *message, void* user_ptr)
    {
        HContext context = (HContext) user_ptr;
        if (message->m_Receiver.m_FunctionRef)
        {
            lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
            // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, see message.h in dlib
            const int ref = message->m_Receiver.m_FunctionRef + LUA_NOREF;

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            assert(lua_isfunction(L, -1));
            lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextReference);

            dmScript::PushHash(L, message->m_Id);

            if (message->m_Descriptor)
            {
                dmScript::PushDDF(L, (dmDDF::Descriptor*)message->m_Descriptor, (const char*) message->m_Data, true);
            }
            else if (message->m_DataSize > 0)
            {
                dmScript::PushTable(L, (const char*) message->m_Data, message->m_DataSize);
            }
            else
            {
                lua_newtable(L);
            }
            dmScript::PCall(L, 3, LUA_MULTRET);
            dmScript::Unref(L, LUA_REGISTRYINDEX, ref);
        }
        else if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
            if (descriptor == dmTrackingDDF::TrackingEvent::m_DDFDescriptor)
            {
                lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
                lua_getglobal(L, "on_event");
                dmScript::PushDDF(L, descriptor, (const char*)message->m_Data, true);
                if (dmScript::PCall(L, 1, 0) != 0)
                {
                    dmLogWarning("PCall failed when dispatching event");
                }
            }
        }
    }

    void Update(HContext context, float dt)
    {
        lua_State* L = dmScript::GetLuaState(context->m_ScriptCtx);
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextReference);
        dmScript::SetInstance(L);

        dmMessage::Dispatch(context->m_Socket, DispatchCallback, (void*)context);

        const int top = lua_gettop(L);
        lua_getglobal(L, "update");
        lua_pushnumber(L, dt);
        int ret = dmScript::PCall(L, 1, 0);
        if (ret != 0)
        {
            dmLogWarning("Tracking update did not complete without errors.");
        }

        assert(top == lua_gettop(L));
        lua_pushnil(L);
        dmScript::SetInstance(L);
    }

    void PostSimpleEvent(HContext context, const char *type)
    {
        uint32_t sz = sizeof(dmTrackingDDF::TrackingEvent) + strlen(type) + 1;
        dmTrackingDDF::TrackingEvent* evt = (dmTrackingDDF::TrackingEvent*) malloc(sz);
        memset(evt, 0x00, sz);
        evt->m_Type = (const char*) sizeof(dmTrackingDDF::TrackingEvent);
        strcpy((char*)&evt[1], type);

        dmMessage::URL url;
        url.m_Socket = context->m_Socket;
        dmMessage::Post(0, &url, dmTrackingDDF::TrackingEvent::m_DDFHash, 0, (uintptr_t) dmTrackingDDF::TrackingEvent::m_DDFDescriptor, evt, sz, 0);
        free(evt);
    }

    // Used to ctxall hooks for scripted tests.
    dmScript::HContext GetScriptContext(HContext context)
    {
        return context->m_ScriptCtx;
    }
}
