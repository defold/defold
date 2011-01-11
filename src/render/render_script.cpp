#include "render_script.h"

#include <string.h>
#include <new>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/profile.h>

#include <script/script.h>

namespace dmRender
{
    #define RENDER_SCRIPT_SOCKET_NAME "render"
    #define RENDER_SCRIPT_INSTANCE "RenderScriptInstance"

    #define RENDER_SCRIPT_LIB_NAME "render"
    #define RENDER_SCRIPT_FORMAT_NAME "format"
    #define RENDER_SCRIPT_WIDTH_NAME "width"
    #define RENDER_SCRIPT_HEIGHT_NAME "height"
    #define RENDER_SCRIPT_MIN_FILTER_NAME "min_filter"
    #define RENDER_SCRIPT_MAG_FILTER_NAME "mag_filter"
    #define RENDER_SCRIPT_U_WRAP_NAME "u_wrap"
    #define RENDER_SCRIPT_V_WRAP_NAME "v_wrap"

    enum RenderScriptFunction
    {
        RENDER_SCRIPT_FUNCTION_INIT,
        RENDER_SCRIPT_FUNCTION_UPDATE,
        RENDER_SCRIPT_FUNCTION_ONMESSAGE,
        MAX_RENDER_SCRIPT_FUNCTION_COUNT
    };

    struct RenderScript
    {
        int m_FunctionReferences[MAX_RENDER_SCRIPT_FUNCTION_COUNT];
    };

    const char* RENDER_SCRIPT_FUNCTION_NAMES[MAX_RENDER_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "update",
        "on_message"
    };

    RenderScriptInstance::RenderScriptInstance()
    : m_CommandBuffer()
    , m_Materials()
    , m_RenderContext(0)
    , m_RenderScript(0)
    , m_PredicateCount(0)
    , m_InstanceReference(0)
    , m_RenderScriptDataReference(0)
    {
        memset(m_Predicates, 0, sizeof(Predicate*) * MAX_PREDICATE_COUNT);
    }

    RenderScriptInstance::~RenderScriptInstance()
    {
        for (uint32_t i = 0; i < m_PredicateCount; ++i)
            delete m_Predicates[i];
    }

    static RenderScriptInstance* RenderScriptInstance_Check(lua_State *L, int index)
    {
        RenderScriptInstance* i;
        luaL_checktype(L, index, LUA_TUSERDATA);
        i = (RenderScriptInstance*)luaL_checkudata(L, index, RENDER_SCRIPT_INSTANCE);
        if (i == NULL) luaL_typerror(L, index, RENDER_SCRIPT_INSTANCE);
        return i;
    }

    static int RenderScriptInstance_gc (lua_State *L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        memset(i, 0, sizeof(*i));
        (void) i;
        assert(i);
        return 0;
    }

    static int RenderScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "GameObject: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int RenderScriptInstance_index(lua_State *L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        assert(i);

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int RenderScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);

        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static const luaL_reg RenderScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScriptInstance_meta[] =
    {
        {"__gc",        RenderScriptInstance_gc},
        {"__tostring",  RenderScriptInstance_tostring},
        {"__index",     RenderScriptInstance_index},
        {"__newindex",  RenderScriptInstance_newindex},
        {0, 0}
    };

    bool InsertCommand(RenderScriptInstance* i, const Command& command)
    {
        if (i->m_CommandBuffer.Full())
            return false;
        else
            i->m_CommandBuffer.Push(command);
        return true;
    }

    int RenderScript_EnableState(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t state = luaL_checknumber(L, 2);

        switch (state)
        {
            case dmGraphics::STATE_DEPTH_TEST:
            case dmGraphics::STATE_ALPHA_TEST:
            case dmGraphics::STATE_BLEND:
            case dmGraphics::STATE_CULL_FACE:
            case dmGraphics::STATE_POLYGON_OFFSET_FILL:
            case dmGraphics::STATE_POLYGON_OFFSET_LINE:
            case dmGraphics::STATE_POLYGON_OFFSET_POINT:
                break;
            default:
                return luaL_error(L, "Invalid state: %s.enable_state(%d).", RENDER_SCRIPT_LIB_NAME, state);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_STATE, state)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_DisableState(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t state = luaL_checknumber(L, 2);
        switch (state)
        {
            case dmGraphics::STATE_DEPTH_TEST:
            case dmGraphics::STATE_ALPHA_TEST:
            case dmGraphics::STATE_BLEND:
            case dmGraphics::STATE_CULL_FACE:
            case dmGraphics::STATE_POLYGON_OFFSET_FILL:
            case dmGraphics::STATE_POLYGON_OFFSET_LINE:
            case dmGraphics::STATE_POLYGON_OFFSET_POINT:
                break;
            default:
                luaL_error(L, "Invalid state: %s.disable_state(%d).", RENDER_SCRIPT_LIB_NAME, state);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_STATE, state)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetViewport(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        int32_t x = luaL_checknumber(L, 2);
        int32_t y = luaL_checknumber(L, 3);
        int32_t width = luaL_checknumber(L, 2);
        int32_t height = luaL_checknumber(L, 3);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_VIEWPORT, x, y, width, height)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_RenderTarget(lua_State* L)
    {
        int top = lua_gettop(L);

        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);

        const char* name = luaL_checkstring(L, 2);

        uint32_t buffer_type_flags = 0;
        luaL_checktype(L, 3, LUA_TTABLE);
        dmGraphics::TextureParams params[dmGraphics::MAX_BUFFER_TYPE_COUNT];
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            uint32_t buffer_type = (uint32_t)luaL_checknumber(L, -2);
            buffer_type_flags |= buffer_type;
            uint32_t index = dmGraphics::GetBufferTypeIndex((dmGraphics::BufferType)buffer_type);
            dmGraphics::TextureParams* p = &params[index];
            luaL_checktype(L, -1, LUA_TTABLE);
            lua_pushnil(L);
            while (lua_next(L, -2))
            {
                const char* key = luaL_checkstring(L, -2);
                if (strncmp(key, RENDER_SCRIPT_FORMAT_NAME, strlen(RENDER_SCRIPT_FORMAT_NAME)) == 0)
                {
                    p->m_Format = (dmGraphics::TextureFormat)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_WIDTH_NAME, strlen(RENDER_SCRIPT_WIDTH_NAME)) == 0)
                {
                    p->m_Width = luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_HEIGHT_NAME, strlen(RENDER_SCRIPT_HEIGHT_NAME)) == 0)
                {
                    p->m_Height = luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_MIN_FILTER_NAME, strlen(RENDER_SCRIPT_MIN_FILTER_NAME)) == 0)
                {
                    p->m_MinFilter = (dmGraphics::TextureFilter)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_MAG_FILTER_NAME, strlen(RENDER_SCRIPT_MAG_FILTER_NAME)) == 0)
                {
                    p->m_MagFilter = (dmGraphics::TextureFilter)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_U_WRAP_NAME, strlen(RENDER_SCRIPT_U_WRAP_NAME)) == 0)
                {
                    p->m_UWrap = (dmGraphics::TextureWrap)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_V_WRAP_NAME, strlen(RENDER_SCRIPT_V_WRAP_NAME)) == 0)
                {
                    p->m_VWrap = (dmGraphics::TextureWrap)(int)luaL_checknumber(L, -1);
                }
                else
                {
                    lua_pop(L, 2);
                    assert(top == lua_gettop(L));
                    return luaL_error(L, "Unknown key supplied to %s.rendertarget: %s. Available keys are: %s, %s, %s, %s, %s, %s, %s.",
                        RENDER_SCRIPT_LIB_NAME, key,
                        RENDER_SCRIPT_FORMAT_NAME,
                        RENDER_SCRIPT_WIDTH_NAME,
                        RENDER_SCRIPT_HEIGHT_NAME,
                        RENDER_SCRIPT_MIN_FILTER_NAME,
                        RENDER_SCRIPT_MAG_FILTER_NAME,
                        RENDER_SCRIPT_U_WRAP_NAME,
                        RENDER_SCRIPT_V_WRAP_NAME);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }

        dmGraphics::HRenderTarget render_target = dmGraphics::NewRenderTarget(i->m_RenderContext->m_GraphicsContext, buffer_type_flags, params);
        RegisterRenderTarget(i->m_RenderContext, render_target, dmHashString64(name));

        lua_pushlightuserdata(L, (void*)render_target);

        return 1;
    }

    int RenderScript_EnableRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 2))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 2);
        }
        if (render_target == 0x0)
            return luaL_error(L, "Invalid render target (nil) supplied to %s.enable_render_target.", RENDER_SCRIPT_LIB_NAME);

        if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_RENDER_TARGET, (uint32_t)render_target)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_DisableRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 2))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 2);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_RENDER_TARGET, (uint32_t)render_target)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_EnableTexture(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        dmGraphics::HRenderTarget render_target = 0x0;

        uint32_t unit = luaL_checknumber(L, 2);
        if (lua_islightuserdata(L, 3))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 3);
            dmGraphics::BufferType buffer_type = (dmGraphics::BufferType)(int)luaL_checknumber(L, 4);
            dmGraphics::HTexture texture = dmGraphics::GetRenderTargetTexture(render_target, buffer_type);
            if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_TEXTURE, unit, (uint32_t)texture)))
                return 0;
            else
                return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
        else
        {
            return luaL_error(L, "%s.enable_texture(self, unit, render_target) called with illegal parameters.", RENDER_SCRIPT_LIB_NAME);
        }
    }

    int RenderScript_DisableTexture(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t unit = luaL_checknumber(L, 2);
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_TEXTURE, unit)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_Clear(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);

        int top = lua_gettop(L);

        uint32_t flags = 0;

        Vectormath::Aos::Vector4 color(0.0f, 0.0f, 0.0f, 0.0f);
        float depth = 0.0f;
        uint32_t stencil = 0;

        lua_pushnil(L);
        while (lua_next(L, 2))
        {
            uint32_t buffer_type = luaL_checknumber(L, -2);
            flags |= buffer_type;
            switch (buffer_type)
            {
                case dmGraphics::BUFFER_TYPE_COLOR_BIT:
                    color = *dmScript::CheckVector4(L, -1);
                    break;
                case dmGraphics::BUFFER_TYPE_DEPTH_BIT:
                    depth = (float)luaL_checknumber(L, -1);
                    break;
                case dmGraphics::BUFFER_TYPE_STENCIL_BIT:
                    stencil = (uint32_t)luaL_checknumber(L, -1);
                    break;
                default:
                    lua_pop(L, 2);
                    assert(top == lua_gettop(L));
                    return luaL_error(L, "Unknown buffer type supplied to %s.clear.", RENDER_SCRIPT_LIB_NAME);
            }
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));

        uint32_t clear_color = 0;
        clear_color |= ((uint8_t)(color.getX() * 255.0f)) << 0;
        clear_color |= ((uint8_t)(color.getY() * 255.0f)) << 8;
        clear_color |= ((uint8_t)(color.getZ() * 255.0f)) << 16;
        clear_color |= ((uint8_t)(color.getW() * 255.0f)) << 24;

        union float_to_uint32_t {float f; uint32_t i;};
        float_to_uint32_t ftoi;
        ftoi.f = depth;
        if (InsertCommand(i, Command(COMMAND_TYPE_CLEAR, flags, clear_color, ftoi.i, stencil)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_Draw(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        dmRender::Predicate* predicate = 0x0;
        if (lua_islightuserdata(L, 2))
        {
            predicate = (dmRender::Predicate*)lua_touserdata(L, 2);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_DRAW, (uint32_t)predicate)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_DrawDebug3d(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        if (InsertCommand(i, Command(COMMAND_TYPE_DRAW_DEBUG3D)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_DrawDebug2d(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        if (InsertCommand(i, Command(COMMAND_TYPE_DRAW_DEBUG2D)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetView(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        Vectormath::Aos::Matrix4 view = *dmScript::CheckMatrix4(L, 2);

        Vectormath::Aos::Matrix4* matrix = new Vectormath::Aos::Matrix4;
        *matrix = view;
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_VIEW, (uint32_t)matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetProjection(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        Vectormath::Aos::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);
        Vectormath::Aos::Matrix4* matrix = new Vectormath::Aos::Matrix4;
        *matrix = projection;
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_PROJECTION, (uint32_t)matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetBlendFunc(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t factors[2];
        for (uint32_t i = 0; i < 2; ++i)
        {
            factors[i] = luaL_checknumber(L, 2+i);
        }
        for (uint32_t i = 0; i < 2; ++i)
        {
            switch (factors[i])
            {
                case dmGraphics::BLEND_FACTOR_ZERO:
                case dmGraphics::BLEND_FACTOR_ONE:
                case dmGraphics::BLEND_FACTOR_SRC_COLOR:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
                case dmGraphics::BLEND_FACTOR_DST_COLOR:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR:
                case dmGraphics::BLEND_FACTOR_SRC_ALPHA:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
                case dmGraphics::BLEND_FACTOR_DST_ALPHA:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
                case dmGraphics::BLEND_FACTOR_SRC_ALPHA_SATURATE:
                case dmGraphics::BLEND_FACTOR_CONSTANT_COLOR:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
                case dmGraphics::BLEND_FACTOR_CONSTANT_ALPHA:
                case dmGraphics::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
                    break;
                default:
                    luaL_error(L, "Invalid blend types: %s.set_blend_func(self, %d, %d)", RENDER_SCRIPT_LIB_NAME, factors[0], factors[1]);
            }
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_BLEND_FUNC, factors[0], factors[1])))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetColorMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);

        if (lua_isboolean(L, 2) && lua_isboolean(L, 3) && lua_isboolean(L, 4) && lua_isboolean(L, 5))
        {
            bool red = lua_toboolean(L, 2) != 0;
            bool green = lua_toboolean(L, 3) != 0;
            bool blue = lua_toboolean(L, 4) != 0;
            bool alpha = lua_toboolean(L, 5) != 0;
            if (!InsertCommand(i, Command(COMMAND_TYPE_SET_COLOR_MASK, (uint32_t)red, (uint32_t)green, (uint32_t)blue, (uint32_t)alpha)))
                return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
        else
        {
            return luaL_error(L, "Expected booleans but got %s, %s, %s, %s.", lua_typename(L, lua_type(L, 2)), lua_typename(L, lua_type(L, 3)), lua_typename(L, lua_type(L, 4)), lua_typename(L, lua_type(L, 5)));
        }
        return 0;
    }

    int RenderScript_SetDepthMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);

        if (lua_isboolean(L, 2))
        {
            bool mask = lua_toboolean(L, 2) != 0;
            if (!InsertCommand(i, Command(COMMAND_TYPE_SET_DEPTH_MASK, (uint32_t)mask)))
                return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
        else
        {
            return luaL_error(L, "Expected boolean but got %s.", lua_typename(L, lua_type(L, 2)));
        }
        return 0;
    }

    int RenderScript_SetStencilMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);

        uint32_t mask = (uint32_t)luaL_checknumber(L, 2);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_STENCIL_MASK, mask)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetCullFace(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t face_type = luaL_checknumber(L, 2);
        switch (face_type)
        {
            case dmGraphics::FACE_TYPE_FRONT:
            case dmGraphics::FACE_TYPE_BACK:
            case dmGraphics::FACE_TYPE_FRONT_AND_BACK:
                break;
            default:
                return luaL_error(L, "Invalid face types: %s.set_cull_face(self, %d)", RENDER_SCRIPT_LIB_NAME, face_type);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_CULL_FACE, (uint32_t)face_type)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_SetPolygonOffset(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        float factor = luaL_checknumber(L, 2);
        float units = luaL_checknumber(L, 3);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_POLYGON_OFFSET, (uint32_t)factor, (uint32_t)units)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    int RenderScript_GetWindowWidth(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowWidth(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    int RenderScript_GetWindowHeight(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowHeight(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    int RenderScript_Predicate(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);
        if (i->m_PredicateCount < MAX_PREDICATE_COUNT)
        {
            dmRender::Predicate* predicate = new dmRender::Predicate();
            i->m_Predicates[i->m_PredicateCount++] = predicate;
            lua_pushnil(L);  /* first key */
            while (lua_next(L, 2) != 0)
            {
                const char* tag = luaL_checkstring(L, -1);
                predicate->m_Tags[predicate->m_TagCount++] = dmHashString32(tag);
                lua_pop(L, 1);
                if (predicate->m_TagCount == dmRender::Predicate::MAX_TAG_COUNT)
                    break;
            }
            lua_pushlightuserdata(L, (void*)predicate);
        }
        return 1;
    }

    int RenderScript_EnableVertexConstant(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        Vectormath::Aos::Vector4* v = dmScript::CheckVector4(L, 3);
        if (!InsertCommand(i, Command(COMMAND_TYPE_ENABLE_VERTEX_CONSTANT, (uint32_t)reg, (uint32_t)new Vectormath::Aos::Vector4(*v))))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_DisableVertexConstant(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        if (!InsertCommand(i, Command(COMMAND_TYPE_DISABLE_VERTEX_CONSTANT, (uint32_t)reg, (uint32_t)0)))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_EnableVertexConstantBlock(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        Vectormath::Aos::Matrix4* m = dmScript::CheckMatrix4(L, 3);
        if (!InsertCommand(i, Command(COMMAND_TYPE_ENABLE_VERTEX_CONSTANT_BLOCK, (uint32_t)reg, (uint32_t)new Vectormath::Aos::Matrix4(*m))))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_DisableVertexConstantBlock(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        if (!InsertCommand(i, Command(COMMAND_TYPE_DISABLE_VERTEX_CONSTANT_BLOCK, (uint32_t)reg, (uint32_t)0)))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_EnableFragmentConstant(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        Vectormath::Aos::Vector4* v = dmScript::CheckVector4(L, 3);
        if (!InsertCommand(i, Command(COMMAND_TYPE_ENABLE_FRAGMENT_CONSTANT, (uint32_t)reg, (uint32_t)new Vectormath::Aos::Vector4(*v))))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_DisableFragmentConstant(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        if (!InsertCommand(i, Command(COMMAND_TYPE_DISABLE_FRAGMENT_CONSTANT, (uint32_t)reg, (uint32_t)0)))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_EnableFragmentConstantBlock(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        Vectormath::Aos::Matrix4* m = dmScript::CheckMatrix4(L, 3);
        if (!InsertCommand(i, Command(COMMAND_TYPE_ENABLE_FRAGMENT_CONSTANT_BLOCK, (uint32_t)reg, (uint32_t)new Vectormath::Aos::Matrix4(*m))))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_DisableFragmentConstantBlock(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t reg = luaL_checknumber(L, 2);
        if (!InsertCommand(i, Command(COMMAND_TYPE_DISABLE_FRAGMENT_CONSTANT_BLOCK, (uint32_t)reg, (uint32_t)0)))
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        return 0;
    }

    int RenderScript_EnableMaterial(lua_State* L)
    {
        int top = lua_gettop(L);
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        if (!lua_isnil(L, 2))
        {
            const char* material_id = luaL_checkstring(L, 2);
            dmRender::HMaterial* mat = i->m_Materials.Get(dmHashString64(material_id));
            if (mat == 0x0)
            {
                return luaL_error(L, "Could not find material '%s'.", material_id);
            }
            else
            {
                HMaterial material = *mat;
                if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_MATERIAL, (uint32_t)material)))
                    return 0;
                else
                    return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
            }
        }
        else
        {
            return luaL_error(L, "%s.enable_material was supplied nil as material.", RENDER_SCRIPT_LIB_NAME);
        }
        assert(top == lua_gettop(L));
    }

    int RenderScript_DisableMaterial(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_MATERIAL)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    static const luaL_reg RenderScript_methods[] =
    {
        {"enable_state",                    RenderScript_EnableState},
        {"disable_state",                   RenderScript_DisableState},
        {"enable_render_target",            RenderScript_EnableRenderTarget},
        {"disable_render_target",           RenderScript_DisableRenderTarget},
        {"render_target",                   RenderScript_RenderTarget},
        {"enable_texture",                  RenderScript_EnableTexture},
        {"disable_texture",                 RenderScript_DisableTexture},
        {"clear",                           RenderScript_Clear},
        {"set_viewport",                    RenderScript_SetViewport},
        {"set_view",                        RenderScript_SetView},
        {"set_projection",                  RenderScript_SetProjection},
        {"set_blend_func",                  RenderScript_SetBlendFunc},
        {"set_color_mask",                  RenderScript_SetColorMask},
        {"set_depth_mask",                  RenderScript_SetDepthMask},
        {"set_stencil_mask",                RenderScript_SetStencilMask},
        {"set_cull_face",                   RenderScript_SetCullFace},
        {"set_polygon_offset",              RenderScript_SetPolygonOffset},
        {"draw",                            RenderScript_Draw},
        {"draw_debug3d",                    RenderScript_DrawDebug3d},
        {"draw_debug2d",                    RenderScript_DrawDebug2d},
        {"get_window_width",                RenderScript_GetWindowWidth},
        {"get_window_height",               RenderScript_GetWindowHeight},
        {"predicate",                       RenderScript_Predicate},
        {"enable_vertex_constant",          RenderScript_EnableVertexConstant},
        {"disable_vertex_constant",         RenderScript_DisableVertexConstant},
        {"enable_vertex_constant_block",    RenderScript_EnableVertexConstantBlock},
        {"disable_vertex_constant_block",   RenderScript_DisableVertexConstantBlock},
        {"enable_fragment_constant",        RenderScript_EnableFragmentConstant},
        {"disable_fragment_constant",       RenderScript_DisableFragmentConstant},
        {"enable_fragment_constant_block",  RenderScript_EnableFragmentConstantBlock},
        {"disable_fragment_constant_block", RenderScript_DisableFragmentConstantBlock},
        {"enable_material",                 RenderScript_EnableMaterial},
        {"disable_material",                RenderScript_DisableMaterial},
        {0, 0}
    };

    void InitializeRenderScriptContext(RenderScriptContext& context, dmMessage::DispatchCallback dispatch_callback, uint32_t command_buffer_size)
    {
        dmMessage::NewSocket(RENDER_SCRIPT_SOCKET_NAME, &context.m_Socket);
        context.m_DispatchCallback = dispatch_callback;
        context.m_CommandBufferSize = command_buffer_size;

        lua_State *L = lua_open();
        context.m_LuaState = L;

        int top = lua_gettop(L);

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        // Pop all stack values generated from luaopen_*
        lua_pop(L, lua_gettop(L));

        luaL_register(L, RENDER_SCRIPT_INSTANCE, RenderScriptInstance_methods);   // create methods table, add it to the globals
        int methods = lua_gettop(L);
        luaL_newmetatable(L, RENDER_SCRIPT_INSTANCE);
        int metatable = lua_gettop(L);
        luaL_register(L, 0, RenderScriptInstance_meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods);                       // dup methods table
        lua_settable(L, metatable);

        lua_pop(L, 2);

        luaL_register(L, RENDER_SCRIPT_LIB_NAME, RenderScript_methods);

        #define REGISTER_RENDER_CONSTANT(name)\
        lua_pushliteral(L, #name);\
        lua_pushnumber(L, dmGraphics::name);\
        lua_settable(L, -3);

        REGISTER_RENDER_CONSTANT(STATE_DEPTH_TEST);
        REGISTER_RENDER_CONSTANT(STATE_ALPHA_TEST);
        REGISTER_RENDER_CONSTANT(STATE_BLEND);
        REGISTER_RENDER_CONSTANT(STATE_CULL_FACE);
        REGISTER_RENDER_CONSTANT(STATE_POLYGON_OFFSET_FILL);
        REGISTER_RENDER_CONSTANT(STATE_POLYGON_OFFSET_LINE);
        REGISTER_RENDER_CONSTANT(STATE_POLYGON_OFFSET_POINT);

        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_LUMINANCE);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGB);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGBA);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGB_DXT1);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGBA_DXT1);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGBA_DXT3);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_RGBA_DXT5);
        REGISTER_RENDER_CONSTANT(TEXTURE_FORMAT_DEPTH);

        REGISTER_RENDER_CONSTANT(TEXTURE_FILTER_LINEAR);
        REGISTER_RENDER_CONSTANT(TEXTURE_FILTER_NEAREST);

        REGISTER_RENDER_CONSTANT(TEXTURE_WRAP_CLAMP);
        REGISTER_RENDER_CONSTANT(TEXTURE_WRAP_CLAMP_TO_BORDER);
        REGISTER_RENDER_CONSTANT(TEXTURE_WRAP_CLAMP_TO_EDGE);
        REGISTER_RENDER_CONSTANT(TEXTURE_WRAP_MIRRORED_REPEAT);
        REGISTER_RENDER_CONSTANT(TEXTURE_WRAP_REPEAT);

        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ZERO);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_SRC_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_DST_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_DST_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_SRC_ALPHA);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_DST_ALPHA);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_SRC_ALPHA_SATURATE);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_CONSTANT_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_CONSTANT_ALPHA);
        REGISTER_RENDER_CONSTANT(BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);

        REGISTER_RENDER_CONSTANT(FACE_TYPE_FRONT);
        REGISTER_RENDER_CONSTANT(FACE_TYPE_BACK);
        REGISTER_RENDER_CONSTANT(FACE_TYPE_FRONT_AND_BACK);

        REGISTER_RENDER_CONSTANT(BUFFER_TYPE_COLOR_BIT);
        REGISTER_RENDER_CONSTANT(BUFFER_TYPE_DEPTH_BIT);
        REGISTER_RENDER_CONSTANT(BUFFER_TYPE_STENCIL_BIT);

        #undef REGISTER_RENDER_CONSTANT

        lua_pop(L, 1);

        dmScript::Initialize(L);

        assert(top == lua_gettop(L));


    }

    void FinalizeRenderScriptContext(RenderScriptContext& context)
    {
        if (context.m_Socket)
            dmMessage::DeleteSocket(context.m_Socket);

        if (context.m_LuaState)
            lua_close(context.m_LuaState);
        context.m_LuaState = 0;
    }

    struct LuaData
    {
        const char* m_Buffer;
        uint32_t m_Size;
    };

    const char* ReadScript(lua_State *L, void *data, size_t *size)
    {
        LuaData* lua_data = (LuaData*)data;
        if (lua_data->m_Size == 0)
        {
            return 0x0;
        }
        else
        {
            *size = lua_data->m_Size;
            lua_data->m_Size = 0;
            return lua_data->m_Buffer;
        }
    }

    static bool LoadRenderScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename, RenderScript* script)
    {
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;

        bool result = false;
        int top = lua_gettop(L);
        (void) top;

        LuaData data;
        data.m_Buffer = (const char*)buffer;
        data.m_Size = buffer_size;
        int ret = lua_load(L, &ReadScript, &data, filename);
        if (ret == 0)
        {
            ret = lua_pcall(L, 0, LUA_MULTRET, 0);
            if (ret == 0)
            {
                for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, RENDER_SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = luaL_ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", RENDER_SCRIPT_FUNCTION_NAMES[i], filename);
                            lua_pop(L, 1);
                            goto bail;
                        }
                    }
                    else
                    {
                        script->m_FunctionReferences[i] = LUA_NOREF;
                        lua_pop(L, 1);
                    }
                }
                result = true;
            }
            else
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
            }
        }
        else
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
bail:
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
        {
            lua_pushnil(L);
            lua_setglobal(L, RENDER_SCRIPT_FUNCTION_NAMES[i]);
        }
        assert(top == lua_gettop(L));
        return result;
    }

    HRenderScript NewRenderScript(HRenderContext render_context, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = render_context->m_RenderScriptContext.m_LuaState;

        RenderScript temp_render_script;
        if (LoadRenderScript(L, buffer, buffer_size, filename, &temp_render_script))
        {
            HRenderScript render_script = new RenderScript();
            *render_script = temp_render_script;
            return render_script;
        }
        else
        {
            return 0;
        }
    }

    bool ReloadRenderScript(HRenderContext render_context, HRenderScript render_script, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        return LoadRenderScript(render_context->m_RenderScriptContext.m_LuaState, buffer, buffer_size, filename, render_script);
    }

    void DeleteRenderScript(HRenderContext render_context, HRenderScript render_script)
    {
        lua_State* L = render_context->m_RenderScriptContext.m_LuaState;
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (render_script->m_FunctionReferences[i])
                luaL_unref(L, LUA_REGISTRYINDEX, render_script->m_FunctionReferences[i]);
        }
        delete render_script;
    }

    HRenderScriptInstance NewRenderScriptInstance(dmRender::HRenderContext render_context, HRenderScript render_script)
    {
        lua_State* L = render_context->m_RenderScriptContext.m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__instances__");

        RenderScriptInstance* i = new (lua_newuserdata(L, sizeof(RenderScriptInstance))) RenderScriptInstance();
        i->m_PredicateCount = 0;
        i->m_RenderScript = render_script;
        i->m_RenderContext = render_context;
        i->m_CommandBuffer.SetCapacity(render_context->m_RenderScriptContext.m_CommandBufferSize);
        i->m_Materials.SetCapacity(16, 8);

        lua_pushvalue(L, -1);
        i->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_RenderScriptDataReference = luaL_ref( L, LUA_REGISTRYINDEX );

        luaL_getmetatable(L, RENDER_SCRIPT_INSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance)
    {
        lua_State* L = render_script_instance->m_RenderContext->m_RenderScriptContext.m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        luaL_unref(L, LUA_REGISTRYINDEX, render_script_instance->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, render_script_instance->m_RenderScriptDataReference);

        assert(top == lua_gettop(L));

        render_script_instance->~RenderScriptInstance();
    }

    void SetRenderScriptInstanceRenderScript(HRenderScriptInstance render_script_instance, HRenderScript render_script)
    {
        render_script_instance->m_RenderScript = render_script;
    }

    void AddRenderScriptInstanceMaterial(HRenderScriptInstance render_script_instance, const char* material_name, dmRender::HMaterial material)
    {
        if (render_script_instance->m_Materials.Full())
        {
            uint32_t new_capacity = 2 * render_script_instance->m_Materials.Capacity();
            render_script_instance->m_Materials.SetCapacity(2 * new_capacity, new_capacity);
        }
        render_script_instance->m_Materials.Put(dmHashString64(material_name), material);
    }

    void ClearRenderScriptInstanceMaterials(HRenderScriptInstance render_script_instance)
    {
        render_script_instance->m_Materials.Clear();
    }

    void RelocateMessageStrings(const dmDDF::Descriptor* descriptor, char* buffer, char* data_start)
    {
        for (uint8_t i = 0; i < descriptor->m_FieldCount; ++i)
        {
            dmDDF::FieldDescriptor* field = &descriptor->m_Fields[i];
            uint32_t field_type = field->m_Type;
            switch (field_type)
            {
                case dmDDF::TYPE_MESSAGE:
                    RelocateMessageStrings(field->m_MessageDescriptor, buffer + field->m_Offset, data_start);
                    break;
                case dmDDF::TYPE_STRING:
                    *((uintptr_t*)&buffer[field->m_Offset]) = (uintptr_t)data_start + *((uintptr_t*)(buffer + field->m_Offset));
                    break;
                default:
                    break;
            }
        }
    }

    RenderScriptResult RunScript(HRenderScriptInstance script_instance, RenderScriptFunction script_function, void* args)
    {
        RenderScriptResult result = RENDER_SCRIPT_RESULT_OK;
        HRenderScript script = script_instance->m_RenderScript;
        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_State* L = script_instance->m_RenderContext->m_RenderScriptContext.m_LuaState;
            int top = lua_gettop(L);
            (void) top;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[script_function]);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            int arg_count = 1;

            if (script_function == RENDER_SCRIPT_FUNCTION_ONMESSAGE)
            {
                arg_count = 3;

                Message* message = (Message*)args;
                dmScript::PushHash(L, message->m_Id);
                if (message->m_DDFDescriptor)
                {
                    // adjust char ptrs to global mem space
                    char* data = (char*)message->m_Buffer;
                    RelocateMessageStrings(message->m_DDFDescriptor, data, data);
                    // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                    // lua_cpcall?
                    dmScript::PushDDF(L, message->m_DDFDescriptor, (const char*)message->m_Buffer);
                }
                else if (message->m_BufferSize > 0)
                {
                    dmScript::PushTable(L, (const char*)message->m_Buffer);
                }
                else
                {
                    lua_newtable(L);
                }
            }
            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = RENDER_SCRIPT_RESULT_FAILED;
            }

            assert(top == lua_gettop(L));
        }

        return result;
    }

    RenderScriptResult InitRenderScriptInstance(HRenderScriptInstance instance)
    {
        return RunScript(instance, RENDER_SCRIPT_FUNCTION_INIT, 0x0);
    }

    RenderScriptResult UpdateRenderScriptInstance(HRenderScriptInstance instance)
    {
        DM_PROFILE(RenderScript, "UpdateRSI");
        RenderScriptContext& context = instance->m_RenderContext->m_RenderScriptContext;
        dmMessage::Dispatch(context.m_Socket, context.m_DispatchCallback, (void*)instance);
        instance->m_CommandBuffer.SetSize(0);
        RenderScriptResult result = RunScript(instance, RENDER_SCRIPT_FUNCTION_UPDATE, 0x0);

        if (instance->m_CommandBuffer.Size() > 0)
            ParseCommands(instance->m_RenderContext, &instance->m_CommandBuffer.Front(), instance->m_CommandBuffer.Size());
        return result;
    }

    void OnMessageRenderScriptInstance(HRenderScriptInstance render_script_instance, Message* message)
    {
        RunScript(render_script_instance, RENDER_SCRIPT_FUNCTION_ONMESSAGE, message);
    }
}
