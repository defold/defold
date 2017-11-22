#include "script.h"

#include <assert.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/job.h>
#include "script.h"
#include "script_private.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*# Job API documentation
     *
     * Job scripting functions.
     *
     * @document
     * @name Job
     * @namespace job
     */

    #define SCRIPT_TYPE_NAME_JOB "job"
    #define SCRIPT_TYPE_NAME_JOB_ENTRY "job-entry"

    bool IsJob(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_JOB);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    bool IsJobEntry(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_JOB_ENTRY);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    dmJob::JobEntry CheckJobEntry(lua_State* L, int index)
    {
        dmJob::JobEntry* job_entry = 0x0;
        if (IsJobEntry(L, index))
        {
            job_entry = (dmJob::JobEntry*) lua_touserdata(L, index);
            return *job_entry;
        }

        luaL_typerror(L, index, SCRIPT_TYPE_NAME_JOB_ENTRY);
        return 0;
    }

    static void NullEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
    {
    }

    static void TodoEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
    {
        dmLogWarning("TODOENTRY !!!");
        dmLogWarning("%f, %f, %f", params[0].m_Double, params[1].m_Double, params[2].m_Double);
    }

    static void LuaEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
    {
        lua_State* L = (lua_State*) params[param_count - 2].m_Pointer;
        int top = lua_gettop(L);
        int func_ref =  params[param_count - 1].m_Int;
        lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);

        dmScript::PCall(L, 0, 0);
        dmScript::Unref(L, LUA_REGISTRYINDEX, func_ref);

        assert(top == lua_gettop(L));
    }

    static void HackEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
    {
        dmLogWarning("HACKENTRY !!!");
    }

    #define dist(a, b, c, d) sqrt(double((a - c) * (a - c) + (b - d) * (b - d)))

    static void Plasma(double time, int w, int h, dmBuffer::HBuffer buffer)
    {
        uint8_t* cb;
        uint32_t buf_size;
        dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("rgb"), (void**) &cb, &buf_size);
        if (r != dmBuffer::RESULT_OK) {
            return;
        }
        if (buf_size < w * h * 3) {
            dmLogError("Buffer too small");
            return;
        }

        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                double value = sin(dist(x + time, y, 128.0, 128.0) / 8.0)
                        + sin(dist(x, y, 64.0, 64.0) / 8.0)
                        + sin(dist(x, y + time / 7, 192.0, 64) / 7.0)
                        + sin(dist(x, y, 192.0, 100.0) / 8.0);
                int color = int((4 + value)) * 32;
                uint8_t r = color;
                uint8_t g = color * 2;
                uint8_t b = 255 - color;
                int index = w * y * 3 + x * 3;

                cb[0] = r;
                cb[1] = g;
                cb[2] = b;

                //pset(x, y, ColorRGB(color, color * 2, 255 - color));
            }
        }
    }

    static void PlasmaEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
    {
        double time = params[0].m_Double;
        int w = params[1].m_Double;
        int h = params[2].m_Double;
        dmBuffer::HBuffer buffer = params[3].m_Buffer;
        Plasma(time, w, h, buffer);
    }


    static void Plasma(double time, int ox, int oy, int mw, int mh, int w, int h, dmBuffer::HBuffer buffer)
    {
        uint8_t* cb;
        uint32_t buf_size;
        dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("rgb"), (void**) &cb, &buf_size);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("No RGB stream in buffer");
            return;
        }

        if (buf_size < (w * h * 3)) {

            dmLogError("Buffer too small");
            return;
        }

        for(int y = oy; y < mh; y++) {
            for(int x = ox; x < mw; x++) {
                double value = sin(dist(x + time, y, 128.0, 128.0) / 8.0)
                        + sin(dist(x, y, 64.0, 64.0) / 8.0)
                        + sin(dist(x, y + time / 7, 192.0, 64) / 7.0)
                        + sin(dist(x, y, 192.0, 100.0) / 8.0);
                int color = int((4 + value)) * 32;
                uint8_t r = color;
                uint8_t g = color * 2;
                uint8_t b = 255 - color;
                int index = w * y * 3 + x * 3;

                cb[index + 0] = r;
                cb[index + 1] = g;
                cb[index + 2] = b;
            }
        }
    }

    static void Twirl(double radius, float angle, int w, int h, dmBuffer::HBuffer in_buffer, dmBuffer::HBuffer out_buffer)
    {
        uint8_t *in, *out;
        uint32_t buf_size_in, buf_size_out;
        dmBuffer::Result r;
        dmBuffer::GetStream(in_buffer, dmHashString64("rgb"), (void**) &in, &buf_size_in);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("No RGB stream in buffer");
            return;
        }

        dmBuffer::GetStream(out_buffer, dmHashString64("rgb"), (void**) &out, &buf_size_out);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("No RGB stream in buffer");
            return;
        }

        float icentreX = w * 0.5f;
        float icentreY = h * 0.5f;

        float radius2 = radius * radius;

        for(int y = 0; y < w; y++) {
            for(int x = 0; x < h; x++) {

                int xprim, yprim;

                float dx = x-icentreX;
                float dy = y-icentreY;
                float distance = dx*dx + dy*dy;
                if (distance > radius2) {
                    xprim = x;
                    yprim = y;
                } else {
                    distance = (float)sqrt(distance);
                    float a = (float)atan2(dy, dx) + angle * (radius-distance) / radius;
                    xprim = icentreX + distance*(float)cos(a);
                    yprim = icentreY + distance*(float)sin(a);
                }

                xprim = dmMath::Clamp(xprim, 0, w - 1);
                yprim = dmMath::Clamp(yprim, 0, h - 1);

                int index_in = w * y * 3 + x * 3;
                int index_out = w * yprim * 3 + xprim * 3;

                out[index_out + 0] = out[index_in + 0];
                out[index_out + 1] = out[index_in + 0];
                out[index_out + 2] = out[index_in + 0];
            }
        }
    }

    void PushJob(lua_State* L, dmJob::HJob job);

    dmJob::HJob CheckJob(lua_State* L, int index)
    {
        dmJob::HJob* job = 0x0;
        if (IsJob(L, index))
        {
            job = (dmJob::HJob*)lua_touserdata(L, index);
            return *job;
        }

        luaL_typerror(L, index, SCRIPT_TYPE_NAME_JOB);
        return dmJob::INVALID_JOB;
    }

    int Job_New(lua_State* L)
    {
        int top = lua_gettop(L);

        int entry_type = lua_type(L, 1);

        dmJob::JobEntry job_entry = 0;

        if (entry_type == LUA_TFUNCTION) {
            job_entry = LuaEntry;
        } else if (IsJobEntry(L, 1)) {
            job_entry = CheckJobEntry(L, 1);
        } else if (entry_type == LUA_TNIL) {
            job_entry = NullEntry;
        } else {
            luaL_error(L, "Expected lua function or job-entry");
        }

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);
        lua_pushnil(L);

        dmJob::HJob parent = dmJob::INVALID_JOB;
        if (IsJob(L, 3)) {
            parent = CheckJob(L, 3);
        }

        dmJob::HJob job;
        dmJob::Result r = dmJob::New(job_entry, &job, parent);
        if (r != dmJob::RESULT_OK) {
            luaL_error(L, "Job creation failed (%d)", r);
        }

        //dmJob::SetAutoComplete(job, true);

        while (lua_next(L, -2) != 0)
        {
            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);

            if (value_type == LUA_TNUMBER) {
                dmJob::AddParamDouble(job, lua_tonumber(L, -1));
            } else if (IsBuffer(L, -1)) {
                LuaHBuffer* buffer = CheckBuffer(L, -1);
                // TODO: ref counting and lua
                dmJob::AddParamBuffer(job, buffer->m_Buffer);
            } else {
                luaL_error(L, "Unsupported parameter type (%d)", value_type);
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        if (entry_type == LUA_TFUNCTION) {
            lua_pushvalue(L, 1);
            int f = dmScript::Ref(L, LUA_REGISTRYINDEX);
            dmJob::AddParamPointer(job, L);
            dmJob::AddParamInt(job, f);
            dmJob::SetRunOnMain(job, true);
        }

        PushJob(L, job);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    void PushJob(lua_State* L, dmJob::HJob job)
    {
        int top = lua_gettop(L);

        dmJob::HJob* j = (dmJob::HJob*) lua_newuserdata(L, sizeof(dmJob::HJob));
        *j = job;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_JOB);
        lua_setmetatable(L, -2);
        //lua_pushvalue(L, -1);

        assert(top + 1 == lua_gettop(L));
    }

    void PushJobEntry(lua_State* L, dmJob::JobEntry job_entry)
    {
        int top = lua_gettop(L);

        dmJob::JobEntry* je = (dmJob::JobEntry*) lua_newuserdata(L, sizeof(dmJob::JobEntry));
        *je = job_entry;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_JOB_ENTRY);
        lua_setmetatable(L, -2);
        //lua_pushvalue(L, -1);

        assert(top + 1 == lua_gettop(L));
    }

    int Job_Run(lua_State* L)
    {
        int top = lua_gettop(L);
        dmJob::HJob job = CheckJob(L, 1);
        dmJob::Result r = dmJob::Run(job);
        if (r != dmJob::RESULT_OK) {
            luaL_error(L, "Couldn't start job (%d)", r);
        }

        // TODO: return value?
        lua_pushboolean(L, 1);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    int Job_Wait(lua_State* L)
    {
        int top = lua_gettop(L);
        dmJob::HJob job = CheckJob(L, 1);
        dmJob::Result r = dmJob::Wait(job);
        if (r != dmJob::RESULT_OK) {
            luaL_error(L, "Couldn't wait on job (%d)", r);
        }

        // TODO: return value?
        lua_pushboolean(L, 1);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Job_gc(lua_State *L)
    {
        dmJob::HJob job = CheckJob(L, 1);
        (void)job;
        return 0;
    }

    static int Job_tostring(lua_State* L)
    {
        dmJob::HJob job = CheckJob(L, 1);
        char buffer[64];
        DM_SNPRINTF(buffer, sizeof(buffer), "%s: 0x%p", SCRIPT_TYPE_NAME_JOB, job.m_Job);
        lua_pushstring(L, buffer);
        return 1;
    }

    static const luaL_reg ScriptJob_functions[] =
    {
        {"new", Job_New},
        {"run", Job_Run},
        {"wait", Job_Wait},
        {0, 0}
    };

    static const luaL_reg ScriptJob_methods[] =
    {
        {"__gc", Job_gc},
        {"__tostring", Job_tostring},
        {0, 0}
    };

    static const luaL_reg ScriptJobEntry_methods[] =
    {
        {0, 0}
    };

    void InitializeJob(lua_State* L)
    {
        // TODO: Config
        dmJob::Init(6, 1024);
        dmLogInfo("InitializeJob");

        int top = lua_gettop(L);

        luaL_newmetatable(L, SCRIPT_TYPE_NAME_JOB_ENTRY);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_register(L, NULL, ScriptJobEntry_methods);

        luaL_newmetatable(L, SCRIPT_TYPE_NAME_JOB);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_register(L, NULL, ScriptJob_methods);

        luaL_register(L, "job", ScriptJob_functions);
        PushJobEntry(L, HackEntry);
        //lua_pushstring(L, "HACK!");
        lua_setfield(L, -2, "hack");

        lua_pop(L, 3);
        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}
