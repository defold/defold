#ifndef DMSDK_SCRIPT_H
#define DMSDK_SCRIPT_H

#include <stdint.h>
#include <dmsdk/dlib/buffer.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

namespace dmScript
{
    /**
     *  A utility to make sure we check the Lua stack state before leaving a function. m_Diff is the expected difference of the stack size.
    */
    struct LuaStackCheck
    {
        /// The Lua state to check
        lua_State* m_L;
        /// The current top of the Lua stack (from lua_gettop())
        int m_Top;
        /// The expected difference in stack size when this sctruct goes out of scope
        int m_Diff;
        LuaStackCheck(lua_State* L, int diff);
        ~LuaStackCheck();
    };
    
    /** A helper macro to validate the Lua stack state before leaving a function.
     * Diff is the expected difference of the stack size.
     * @param L     lua state
     * @param diff  (int) Number of expected items to be on the Lua stack once this struct goes out of scope
    */
    #define DM_LUA_STACK_CHECK(_L_, _diff_)     dmScript::LuaStackCheck lua_stack_check(_L_, _diff_);

    /** A wrapper for luaL_ref(L, LUA_REGISTRYINDEX). It also tracks number of global references kept
     * @param L lua state
     * @param table the lua table that stores the references. E.g LUA_REGISTRYINDEX
     * @return the new reference
    */
    int Ref(lua_State* L, int table);

    /** A wrapper for luaL_unref. It also decreases the number of global references kept
     * @param L lua state
     * @param table the lua table that stores the references. E.g LUA_REGISTRYINDEX
     * @param reference the reference to the object
    */
    void Unref(lua_State* L, int table, int reference);

    /**
     * Check if the value at #index is a HBuffer
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a HBuffer
     */
    bool IsBuffer(lua_State *L, int index);

    /**
     * Push a HBuffer onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param buffer HBuffer to push
     */
    void PushBuffer(lua_State* L, dmBuffer::HBuffer buffer);

    /**
     * Check if the value in the supplied index on the lua stack is a HBuffer and returns it.
     * @param L Lua state
     * @param index Index of the value
     * @return The pointer to a HBuffer
     */
    dmBuffer::HBuffer* CheckBuffer(lua_State* L, int index);
}

#endif // DMSDK_SCRIPT_H
