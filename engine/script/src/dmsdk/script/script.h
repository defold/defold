#ifndef DMSDK_SCRIPT_H
#define DMSDK_SCRIPT_H

#include <stdint.h>
#include <stdarg.h>
#include <dmsdk/dlib/buffer.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

namespace dmScript
{
    /*# SDK Script API documentation
     * [file:<dmsdk/script/script.h>]
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Script
     * @namespace dmScript
     */

    /**
    * LuaStackCheck struct. Internal
    *
    * LuaStackCheck utility to make sure we check the Lua stack state before leaving a function.
    * m_Diff is the expected difference of the stack size.
    *
    */
    struct LuaStackCheck
    {
        LuaStackCheck(lua_State* L, int diff);
        ~LuaStackCheck();
        void Verify(int diff);
        #if defined(__GNUC__)
        int Error(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));;
        #else
        int Error(const char* fmt, ...);
        #endif

        /// The Lua state to check
        lua_State* m_L;
        /// The current top of the Lua stack (from lua_gettop())
        int m_Top;
        /// The expected difference in stack size when this sctruct goes out of scope
        int m_Diff;
    };


    /*# helper macro to validate the Lua stack state before leaving a function.
     *
     * Diff is the expected difference of the stack size.
     * If luaL_error, or another function that executes a long-jump, is part of the executed code,
     * the stack guard cannot be guaranteed to execute at the end of the function.
     * In that case you should manually check the stack using `lua_gettop`.
     * In the case of luaL_error, see [ref:DM_LUA_ERROR].
     *
     * @macro
     * @name DM_LUA_STACK_CHECK
     * @param L [type:lua_State*] lua state
     * @param diff [type:int] Number of expected items to be on the Lua stack once this struct goes out of scope
     * @examples
     *
     * ```cpp
     * DM_LUA_STACK_CHECK(L, 1);
     * lua_pushnumber(L, 42);
     * ```
     */
    #define DM_LUA_STACK_CHECK(_L_, _diff_)     dmScript::LuaStackCheck _DM_LuaStackCheck(_L_, _diff_);


    /*# helper macro to validate the Lua stack state and throw a lua error.
     *
     * This macro will verify that the Lua stack size hasn't been changed before
     * throwing a Lua error, which will long-jump out of the current function.
     * This macro can only be used together with [ref:DM_LUA_STACK_CHECK] and should
     * be prefered over manual checking of the stack.
     *
     * @macro
     * @name DM_LUA_ERROR
     * @param fmt [type:const char*] Format string that contains error information.
     * @param args [type:...] Format string args (variable arg list)
     * @examples
     *
     * ```cpp
     * static int ModuleFunc(lua_State* L)
     * {
     *     DM_LUA_STACK_CHECK(L, 1);
     *     if (some_error_check(L))
     *     {
     *         return DM_LUA_ERROR("some error message");
     *     }
     *     lua_pushnumber(L, 42);
     *     return 1;
     * }
     * ```
     */
    #define DM_LUA_ERROR(_fmt_, ...)   _DM_LuaStackCheck.Error(_fmt_,  ##__VA_ARGS__); \


    /*# wrapper for luaL_ref.
     *
     * Creates and returns a reference, in the table at index t, for the object at the
     * top of the stack (and pops the object).
     * It also tracks number of global references kept.
     *
     * @name dmScript::Ref
     * @param L [type:lua_State*] lua state
     * @param table [type:int] table the lua table that stores the references. E.g LUA_REGISTRYINDEX
     * @return reference [type:int] the new reference
     */
    int Ref(lua_State* L, int table);

    /*# wrapper for luaL_unref.
     *
     * Releases reference ref from the table at index t (see luaL_ref).
     * The entry is removed from the table, so that the referred object can be collected.
     * It also decreases the number of global references kept
     *
     * @name dmScript::Unref
     * @param L [type:lua_State*] lua state
     * @param table [type:int] table the lua table that stores the references. E.g LUA_REGISTRYINDEX
     * @param reference [type:int] the reference to the object
     */
    void Unref(lua_State* L, int table, int reference);

    /*#
     * Retrieve current script instance from the global table and place it on the top of the stack, only valid when set.
     * (see [ref:dmScript::GetMainThread])
     * @name dmScript::GetInstance
     * @param L [type:lua_State*] lua state
     */
    void GetInstance(lua_State* L);

    /*#
     * Sets the current script instance
     * Set the value on the top of the stack as the instance into the global table and pops it from the stack.
     * (see [ref:dmScript::GetMainThread])
     * @name dmScript::SetInstance
     * @param L [type:lua_State*] lua state
     */
    void SetInstance(lua_State* L);

    /*#
     * Check if the script instance in the lua state is valid. The instance is assumed to have been previously set by [ref:dmScript::SetInstance].
     * @name dmScript::IsInstanceValid
     * @param L [type:lua_State*] lua state
     * @return boolean [type:bool] Returns true if the instance is valid
     */
    bool IsInstanceValid(lua_State* L);

    /*#
     * Retrieve the main thread lua state from any lua state (main thread or coroutine).
     * @name dmScript::GetMainThread
     * @param L [type:lua_State*] lua state
     * @return lua_State [type:lua_State*] the main thread lua state
     *
     * @examples
     *
     * How to create a Lua callback
     *
     * ```cpp
     * struct LuaCallbackInfo
     * {
     *     LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
     *     lua_State* m_L;
     *     int        m_Callback;
     *     int        m_Self;
     * };
     *
     * static void RegisterCallback(lua_State* L, int index, LuaCallbackInfo* cbk)
     * {
     *     if(cbk->m_Callback != LUA_NOREF)
     *     {
     *         dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
     *         dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
     *     }
     *
     *     cbk->m_L = dmScript::GetMainThread(L);
     *
     *     luaL_checktype(L, index, LUA_TFUNCTION);
     *     lua_pushvalue(L, index);
     *     cbk->m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
     *
     *     dmScript::GetInstance(L);
     *     cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
     * }
     *
     * static void UnregisterCallback(LuaCallbackInfo* cbk)
     * {
     *     if(cbk->m_Callback != LUA_NOREF)
     *     {
     *         dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
     *         dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
     *         cbk->m_Callback = LUA_NOREF;
     *     }
     * }
     *
     * LuaCallbackInfo g_MyCallbackInfo;
     *
     * static void InvokeCallback(LuaCallbackInfo* cbk)
     * {
     *     if(cbk->m_Callback == LUA_NOREF)
     *     {
     *         return;
     *     }
     *
     *     lua_State* L = cbk->m_L;
     *     int top = lua_gettop(L);
     *
     *     lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Callback);
     *
     *     // Setup self (the script instance)
     *     lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Self);
     *     lua_pushvalue(L, -1);
     *
     *     dmScript::SetInstance(L);
     *
     *     lua_pushstring(L, "Hello from extension!");
     *     lua_pushnumber(L, 76);
     *
     *     int number_of_arguments = 3; // instance + 2
     *     int ret = lua_pcall(L, number_of_arguments, 0, 0);
     *     if(ret != 0) {
     *         dmLogError("Error running callback: %s", lua_tostring(L, -1));
     *         lua_pop(L, 1);
     *     }
     *     assert(top == lua_gettop(L));
     * }
     *
     * static int Start(lua_State* L)
     * {
     *     DM_LUA_STACK_CHECK(L, 0);
     *
     *     RegisterCallback(L, 1, &g_MyCallbackInfo);
     *
     *     return 0;
     * }
     *
     * static int Update(lua_State* L)
     * {
     *     DM_LUA_STACK_CHECK(L, 0);
     *
     *     static int count = 0;
     *     if( count++ == 5 )
     *     {
     *         InvokeCallback(&g_MyCallbackInfo);
     *         UnregisterCallback(&g_MyCallbackInfo);
     *     }
     *     return 0;
     * }
     * ```
     */
    lua_State* GetMainThread(lua_State* L);

    /*# Lua wrapper for a dmBuffer::HBuffer
     * 
     * Holds info about the buffer and who owns it.
     *
     * @struct
     * @name dmScript::LuaHBuffer
     * @member m_Buffer [type:dmBuffer::HBuffer]    The buffer
     * @member m_UseLuaGC [type:bool]               If true, it will be garbage collected by Lua. If false, the C++ extension still owns the reference.
     */
    struct LuaHBuffer
    {
        dmBuffer::HBuffer   m_Buffer;
        bool                m_UseLuaGC;     //!< If true, Lua will delete the buffer in the Lua GC phase
    };

    /*# check if the value at #index is a dmScript::LuaHBuffer
     *
     * @name dmScript::IsBuffer
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return boolean [type:boolean] true if value at #index is a LuaHBuffer
     */
    bool IsBuffer(lua_State* L, int index);

    /*# push a LuaHBuffer onto the supplied lua state
     *
     * Will increase the stack by 1.
     *
     * @name dmScript::PushBuffer
     * @param L [type:lua_State*] lua state
     * @param buffer [type:dmScript::LuaHBuffer] buffer to push
     * @examples
     *
     * How to push a buffer and give Lua ownership of the buffer (GC)
     *
     * ```cpp
     * dmScript::LuaHBuffer luabuf = { buffer, true };
     * PushBuffer(L, luabuf);
     * ```
     *
     * How to push a buffer and keep ownership in C++
     *
     * ```cpp
     * dmScript::LuaHBuffer luabuf = { buffer, false };
     * PushBuffer(L, luabuf);
     * ```
     */
    void PushBuffer(lua_State* L, const LuaHBuffer& buffer);

    /*# retrieve a HBuffer from the supplied lua state
     *
     * Check if the value in the supplied index on the lua stack is a HBuffer and returns it.
     *
     * @name dmScript::CheckBuffer
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:LuaHBuffer*] pointer to dmScript::LuaHBuffer
     */
    LuaHBuffer* CheckBuffer(lua_State* L, int index);
}

#endif // DMSDK_SCRIPT_H
