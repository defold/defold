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
    /*# SDK Script API documentation
     * [file:<dmsdk/script/script.h>]
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Script
     * @namespace dmScript
     */

    /*# LuaStackCheck struct
     *
     * LuaStackCheck utility to make sure we check the Lua stack state before leaving a function.
     * m_Diff is the expected difference of the stack size.
     *
     * @struct
     * @name dmScript::LuaStackCheck
     * @member m_L [type:lua_State*] The Lua state to check
     * @member m_Top [type:int] The current top of the Lua stack (from lua_gettop())
     * @member m_Diff [type:int] The expected difference in stack size when this sctruct goes out of scope
     *
     */
    struct LuaStackCheck
    {
        lua_State* m_L;
        int m_Top;
        int m_Diff;
        LuaStackCheck(lua_State* L, int diff);
        ~LuaStackCheck();
    };


    /*# helper macro to validate the Lua stack state before leaving a function.
     *
     * Diff is the expected difference of the stack size.
     *
     * @macro
     * @name DM_LUA_STACK_CHECK
     * @param L [type:lua_State] lua state
     * @param diff [type:int] Number of expected items to be on the Lua stack once this struct goes out of scope
     *
     */
    #define DM_LUA_STACK_CHECK(_L_, _diff_)     dmScript::LuaStackCheck lua_stack_check(_L_, _diff_);


    /*# wrapper for luaL_ref.
     *
     * Creates and returns a reference, in the table at index t, for the object at the
     * top of the stack (and pops the object).
     * It also tracks number of global references kept.
     *
     * @name dmScript::Ref
     * @param L [type:lua_State] lua state
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
     * @param L [type:lua_State] lua state
     * @param table [type:int] table the lua table that stores the references. E.g LUA_REGISTRYINDEX
     * @param reference [type:int] the reference to the object
     */
    void Unref(lua_State* L, int table, int reference);

    /*# check if the value at #index is a HBuffer
     *
     * @name dmScript::IsBuffer
     * @param L [type:lua_State] lua state
     * @param index [type:int] Index of the value
     * @return boolean [type:boolean] true if value at #index is a HBuffer
     */
    bool IsBuffer(lua_State *L, int index);

    /*# push a HBuffer onto the supplied lua state
     *
     * Will increase the stack by 1.
     *
     * @name dmScript::PushBuffer
     * @param L [type:lua_State] lua state
     * @param buffer [type:dmBuffer::HBuffer] buffer to push
     */
    void PushBuffer(lua_State* L, dmBuffer::HBuffer buffer);

    /*# retrieve a HBuffer from the supplied lua state
     *
     * Check if the value in the supplied index on the lua stack is a HBuffer and returns it.
     *
     * @name dmScript::CheckBuffer
     * @param L [type:lua_State] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:dmBuffer::HBuffer*] pointer to dmBuffer::HBuffer
     */
    dmBuffer::HBuffer* CheckBuffer(lua_State* L, int index);
}

#endif // DMSDK_SCRIPT_H
