// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_SCRIPT_SCRIPT_H
#define DMSDK_SCRIPT_SCRIPT_H

#include <stdint.h>
#include <stdarg.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/message.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/ddf/ddf.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

namespace dmScript
{
    /*# SDK Script API documentation
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Script
     * @namespace dmScript
     * @path engine/dlib/src/dmsdk/script/script.h
     */

    /*#
     * The script context
     * @typedef
     * @name HContext
     */
    typedef struct Context* HContext;

    /**
    * LuaStackCheck struct. Internal
    *
    * LuaStackCheck utility to make sure we check the Lua stack state before leaving a function.
    * m_Diff is the expected difference of the stack size.
    *
    */
    struct LuaStackCheck
    {
        LuaStackCheck(lua_State* L, int diff, const char* filename, int linenumber);
        ~LuaStackCheck();
        void Verify(int diff);
        #if defined(__GNUC__)
        int Error(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));;
        #else
        int Error(const char* fmt, ...);
        #endif

        /// The Lua state to check
        lua_State* m_L;

        /// Debug info in case of an assert
        const char* m_Filename;
        int m_Linenumber;

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
    #define DM_LUA_STACK_CHECK(_L_, _diff_)     dmScript::LuaStackCheck _DM_LuaStackCheck(_L_, _diff_, __FILE__, __LINE__);


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
     * dmScript::LuaCallbackInfo* g_MyCallbackInfo = 0;
     *
     * static void InvokeCallback(dmScript::LuaCallbackInfo* cbk)
     * {
     *     if (!dmScript::IsCallbackValid(cbk))
     *         return;
     *
     *     lua_State* L = dmScript::GetCallbackLuaContext(cbk);
     *     DM_LUA_STACK_CHECK(L, 0)
     *
     *     if (!dmScript::SetupCallback(cbk))
     *     {
     *         dmLogError("Failed to setup callback");
     *         return;
     *     }
     *
     *     lua_pushstring(L, "Hello from extension!");
     *     lua_pushnumber(L, 76);
     *
     *     dmScript::PCall(L, 3, 0); // instance + 2
     *
     *     dmScript::TeardownCallback(cbk);
     * }
     *
     * static int Start(lua_State* L)
     * {
     *     DM_LUA_STACK_CHECK(L, 0);
     *
     *     g_MyCallbackInfo = dmScript::CreateCallback(L, 1);
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
     *         InvokeCallback(g_MyCallbackInfo);
     *         if (g_MyCallbackInfo)
     *             dmScript::DestroyCallback(g_MyCallbackInfo);
     *         g_MyCallbackInfo = 0;
     *     }
     *     return 0;
     * }
     * ```
     */
    lua_State* GetMainThread(lua_State* L);

    /*#
     * Retrieve Lua state from the context
     * @param context [type: HContext] the script context
     * @return state [type: lua_State*] the lua state
     */
    lua_State* GetLuaState(HContext context);

    /*# get the value at index as a dmVMath::Vector3*
     * Get the value at index as a dmVMath::Vector3*
     * @name dmScript::ToVector3
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return v [type:dmVMath::Vector3*] The pointer to the value, or 0 if not correct type
     */
    dmVMath::Vector3* ToVector3(lua_State* L, int index);

    /*#
     * Check if the value at #index is a dmVMath::Vector3*
     * @name dmScript::IsVector3
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a dmVMath::Vector3*
     */
    bool IsVector3(lua_State* L, int index);

    /*# push a dmVMath::Vector3 onto the Lua stack
     *
     * Push a dmVMath::Vector3 value onto the supplied lua state, will increase the stack by 1.
     * @name dmScript::PushVector3
     * @param L [type:lua_State*] Lua state
     * @param v [type:dmVMath::Vector3] Vector3 value to push
     */
    void PushVector3(lua_State* L, const dmVMath::Vector3& v);

    /*# check if the value is a dmVMath::Vector3
     *
     * Check if the value in the supplied index on the lua stack is a dmVMath::Vector3.
     * @note throws a luaL_error if it's not the correct type
     * @name dmScript::CheckVector3
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return vector3 [type:dmVMath::Vector3*] The pointer to the value
     */
    dmVMath::Vector3* CheckVector3(lua_State* L, int index);

    /*# get the value at index as a dmVMath::Vector4*
     * Get the value at index as a dmVMath::Vector4*
     * @name dmScript::ToVector4
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return v [type:dmVMath::Vector4*] The pointer to the value, or 0 if not correct type
     */
    dmVMath::Vector4* ToVector4(lua_State* L, int index);

    /*#
     * Check if the value at #index is a dmVMath::Vector4*
     * @name dmScript::IsVector4
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a dmVMath::Vector4*
     */
    bool IsVector4(lua_State* L, int index);

    /*# push a dmVMath::Vector4 on the stack
     * Push a dmVMath::Vector4 value onto the supplied lua state, will increase the stack by 1.
     * @name dmScript::PushVector4
     * @param L [type:lua_State*] Lua state
     * @param v [type:dmVMath::Vector4] dmVMath::Vector4 value to push
     */
    void PushVector4(lua_State* L, const dmVMath::Vector4& v);

    /*# check if the value is a dmVMath::Vector3
     *
     * Check if the value in the supplied index on the lua stack is a dmVMath::Vector3.
     * @note throws a luaL_error if it's not the correct type
     * @name dmScript::CheckVector4
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return vector4 [type:dmVMath::Vector4*] The pointer to the value
     */
    dmVMath::Vector4* CheckVector4(lua_State* L, int index);

    /*# get the value at index as a dmVMath::Quat*
     * Get the value at index as a dmVMath::Quat*
     * @name dmScript::ToQuat
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return quat [type:dmVMath::Quat*] The pointer to the value, or 0 if not correct type
     */
    dmVMath::Quat* ToQuat(lua_State* L, int index);

    /*#
     * Check if the value at #index is a dmVMath::Quat*
     * @name dmScript::IsQuat
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a dmVMath::Quat*
     */
    bool IsQuat(lua_State* L, int index);

    /*# push a dmVMath::Quat onto the Lua stack
     * Push a quaternion value onto Lua stack. Will increase the stack by 1.
     * @name dmScript::PushQuat
     * @param L [type:lua_State*] Lua state
     * @param quat [type:dmVMath::Quat] dmVMath::Quat value to push
     */
    void PushQuat(lua_State* L, const dmVMath::Quat& q);

    /*# check if the value is a dmVMath::Vector3
     *
     * Check if the value in the supplied index on the lua stack is a dmVMath::Quat.
     * @note throws a luaL_error if it's not the correct type
     * @name dmScript::CheckQuat
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return quat [type:dmVMath::Quat*] The pointer to the value
     */
    dmVMath::Quat* CheckQuat(lua_State* L, int index);

    /*# get the value at index as a dmVMath::Matrix4*
     * Get the value at index as a dmVMath::Matrix4*
     * @name dmScript::ToMatrix4
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return quat [type:dmVMath::Matrix4*] The pointer to the value, or 0 if not correct type
     */
    dmVMath::Matrix4* ToMatrix4(lua_State* L, int index);

    /*#
     * Check if the value at #index is a dmVMath::Matrix4*
     * @name dmScript::IsMatrix4
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a dmVMath::Matrix4*
     */
    bool IsMatrix4(lua_State* L, int index);

    /*# push a dmVMath::Matrix4 onto the Lua stack
     * Push a matrix4 value onto the Lua stack. Will increase the stack by 1.
     * @name dmScript::PushMatrix4
     * @param L [type:lua_State*] Lua state
     * @param matrix [type:dmVMath::Matrix4] dmVMath::Matrix4 value to push
     */
    void PushMatrix4(lua_State* L, const dmVMath::Matrix4& m);

    /*# check if the value is a dmVMath::Matrix4
     *
     * Check if the value in the supplied index on the lua stack is a dmVMath::Matrix4.
     *
     * @note throws a luaL_error if it's not the correct type
     * @name dmScript::CheckMatrix4
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return matrix [type:dmVMath::Matrix4*] The pointer to the value
     */
    dmVMath::Matrix4* CheckMatrix4(lua_State* L, int index);

    /*#
     * Check if the value at #index is a hash
     * @name dmScript::IsHash
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return true if the value at #index is a hash
     */
    bool IsHash(lua_State *L, int index);

    /*#
     * Push a hash value onto the supplied lua state, will increase the stack by 1.
     * @name dmScript::PushHash
     * @param L [type:lua_State*] Lua state
     * @param hash [tyoe: dmhash_t] Hash value to push
     */
    void PushHash(lua_State* L, dmhash_t hash);

    /*# get hash value
     * Check if the value in the supplied index on the lua stack is a hash.
     * @name dmScript::CheckHash
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return The hash value
     */
    dmhash_t CheckHash(lua_State* L, int index);

    /*# get hash from hash or string
     * Check if the value in the supplied index on the lua stack is a hash or string.
     * If it is a string, it gets hashed on the fly
     * @name dmScript::CheckHashOrString
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @return The hash value
     */
    dmhash_t CheckHashOrString(lua_State* L, int index);

    /*#
     * Gets as good as possible printable string from a hash or string
     * @name GetStringFromHashOrString
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] Index of the value
     * @param buffer [type: char*] buffer receiving the value
     * @param buffer_length [type: uint32_t] the buffer length
     * @return string [type: const char*] Returns buffer. If buffer is non null, it will always contain a null terminated string. "<unknown>" if the hash could not be looked up.
    */
    const char* GetStringFromHashOrString(lua_State* L, int index, char* buffer, uint32_t bufferlength);

    /*#
     * Push DDF message to Lua stack
     * @param L [type: lua_State*] the Lua state
     * @param descriptor [type: const dmDDF::Descriptor*] field descriptor
     * @param pointers_are_offets [type: bool] True if pointers are offsets
     * @param data [type: const char*] the message data (i.e. the message struct)
     */
    void PushDDF(lua_State*L, const dmDDF::Descriptor* descriptor, const char* data, bool pointers_are_offsets);

    /*# convert a Json string to a Lua table
     * Convert a Json string to Lua table.
     * @note Throws Lua error if it fails to parser the json
     *
     * @name dmScript::JsonToLua
     * @param L [type:lua_State*] lua state
     * @param json [type:const char*] json string
     * @param json_len [type:size_t] length of json string
     * @return int [type:int] 1 if it succeeds. Throws a Lua error if it fails
     */
    int JsonToLua(lua_State* L, const char* json, size_t json_len);

    /*# convert a Lua table to a Json string
     * Convert a Lua table to a Json string
     *
     * @name dmScript::LuaToJson
     * @param L [type:lua_State*] lua state
     * @param json [type:char**] [out] Pointer to char*, which will receive a newly allocated string. Use free().
     * @param json_len [type:size_t*] length of json string
     * @return int [type:int] <0 if it fails. >=0 if it succeeds.
     */
    int LuaToJson(lua_State* L, char** json, size_t* json_len);

    /*# callback info struct
     * callback info struct that will hold the relevant info needed to make a callback into Lua
     * @struct
     * @name dmScript::LuaCallbackInfo
     */
    struct LuaCallbackInfo;

    /*# Register a Lua callback.
     * Stores the current Lua state plus references to the script instance (self) and the callback.
     * Expects SetInstance() to have been called prior to using this method.
     *
     * The allocated data is created on the Lua stack and references are made against the
     * instances own context table.
     *
     * If the callback is not explicitly deleted with DestroyCallback() the references and
     * data will stay around until the script instance is deleted.
     *
     * @name dmScript::CreateCallback
     * @param L Lua state
     * @param index Lua stack index of the function
     * @return Lua callback struct if successful, 0 otherwise
     *
     * @examples
     *
     * ```cpp
     * static int SomeFunction(lua_State* L) // called from Lua
     * {
     *     LuaCallbackInfo* cbk = dmScript::CreateCallback(L, 1);
     *     ... store the callback for later
     * }
     *
     * static void InvokeCallback(LuaCallbackInfo* cbk)
     * {
     *     lua_State* L = dmScript::GetCallbackLuaContext(cbk);
     *     DM_LUA_STACK_CHECK(L, 0);
     *
     *     if (!dmScript::SetupCallback(callback))
     *     {
     *         return;
     *     }
     *
     *     lua_pushstring(L, "hello");
     *
     *     dmScript::PCall(L, 2, 0); // self + # user arguments
     *
     *     dmScript::TeardownCallback(callback);
     *     dmScript::DestroyCallback(cbk); // only do this if you're not using the callback again
     * }
     * ```
     */
    LuaCallbackInfo* CreateCallback(lua_State* L, int index);

    /*# Check if Lua callback is valid.
     * @name dmScript::IsCallbackValid
     * @param cbk Lua callback struct
     */
    bool IsCallbackValid(LuaCallbackInfo* cbk);

    /*# Deletes the Lua callback
     * @name dmScript::DestroyCallback
     * @param cbk Lua callback struct
     */
    void DestroyCallback(LuaCallbackInfo* cbk);

    /*# Gets the Lua context from a callback struct
     * @name dmScript::GetCallbackLuaContext
     * @param cbk Lua callback struct
     * @return L Lua state
     */
    lua_State* GetCallbackLuaContext(LuaCallbackInfo* cbk);


    /*# Setups up the Lua callback prior to a call to dmScript::PCall()
     *  The Lua stack after a successful call:
     * ```
     *    [-4] old instance
     *    [-3] context table
     *    [-2] callback
     *    [-1] self
     * ```
     *  In the event of an unsuccessful call, the Lua stack is unchanged
     *
     * @name dmScript::SetupCallback
     * @param cbk Lua callback struct
     * @return true if the setup was successful
     */
    bool SetupCallback(LuaCallbackInfo* cbk);

    /*# Cleans up the stack after SetupCallback+PCall calls
     * Sets the previous instance
     * Expects Lua stack:
     * ```
     *    [-2] old instance
     *    [-1] context table
     * ```
     * Both values are removed from the stack
     *
     * @name dmScript::TeardownCallback
     * @param cbk Lua callback struct
     */
    void TeardownCallback(LuaCallbackInfo* cbk);

    /*#
     * This function wraps lua_pcall with the addition of specifying an error handler which produces a backtrace.
     * In the case of an error, the error is logged and popped from the stack.
     *
     * @name dmScript::PCall
     * @param L lua state
     * @param nargs number of arguments
     * @param nresult number of results
     * @return error code from pcall
     */
    int PCall(lua_State* L, int nargs, int nresult);

    /*#
     * Creates a reference to the value at top of stack, the ref is done in the
     * current instances context table.
     *
     * Expects SetInstance() to have been set with an value that has a meta table
     * with META_GET_INSTANCE_CONTEXT_TABLE_REF method.
     *
     * @name RefInInstance
     * @param L [type: lua_State*] Lua state
     * @return lua [type: int] ref to value or LUA_NOREF
     *
     * Lua stack on entry
     *  [-1] value
     *
     * Lua stack on exit
    */
    int RefInInstance(lua_State* L);

    /*#
     * Deletes the instance local lua reference
     *
     * Expects SetInstance() to have been set with an value that has a meta table
     * with META_GET_INSTANCE_CONTEXT_TABLE_REF method.
     *
     * @name UnrefInInstance
     * @param L [type: lua_State*] Lua state
     * @param ref [type: int] ref to value or LUA_NOREF
     *
     * Lua stack on entry
     *
     * Lua stack on exit
     */
    void UnrefInInstance(lua_State* L, int ref);

    /*#
     * Resolves the value in the supplied index on the lua stack to a URL. It long jumps (calls luaL_error) on failure.
     * It also gets the current (caller) url if the a pointer is passed to `out_default_url`
     * @param L [type:lua_State*] Lua state
     * @param out_url [type:dmMessage::URL*] where to store the result
     * @param out_default_url [type:dmMessage::URL*] default URL used in the resolve, can be 0x0 (not used)
     * @return result [type:int] 0 if successful. Throws Lua error on failure
     */
    int ResolveURL(lua_State* L, int index, dmMessage::URL* out_url, dmMessage::URL* out_default_url);

    /*#
     * Resolves a url in string format into a dmMessage::URL struct.
     *
     * Special handling for:
     * - "." returns the default socket + path
     * - "#" returns default socket + path + fragment
     *
     * @name RefInInstance
     * @param L [type:lua_State*] Lua state
     * @param url [type:const char*] url
     * @param out_url [type:dmMessage::URL*] where to store the result
     * @param default_url [type:dmMessage::URL*] default url
     * @return result [type:dmMessage::Result] dmMessage::RESULT_OK if the conversion succeeded
    */
    dmMessage::Result ResolveURL(lua_State* L, const char* url, dmMessage::URL* out_url, dmMessage::URL* default_url);

    /*#
     * Converts a URL into a readable string. Useful for e.g. error messages
     * @name UrlToString
     * @param url [type:dmMessage::URL*] url
     * @param buffer [type:char*] the output buffer
     * @param buffer_size [type:uint32_t] the output buffer size
     * @return buffer [type:const char*] returns the passed in buffer
     */
    const char* UrlToString(const dmMessage::URL* url, char* buffer, uint32_t buffer_size);

    /**
     * Get the size of a table when serialized
     * @name CheckTableSize
     * @param L [type: lua_State*] Lua state
     * @param index [type: int] Index of the table
     * @return result [type: uint32_t] Number of bytes required for the serialized table
     */
    uint32_t CheckTableSize(lua_State* L, int index);

    /*#
     * Serialize a table to a buffer
     * Supported types: LUA_TBOOLEAN, LUA_TNUMBER, LUA_TSTRING, Point3, Vector3, Vector4 and Quat
     * Keys must be strings
     * @name CheckTable
     * @param L [type: lua_State*] Lua state
     * @param buffer [type: char*] Buffer that will be written to (must be DM_ALIGNED(16))
     * @param buffer_size [type: uint32_t] Buffer size
     * @param index [type: int] Index of the table
     * @return result [type: uint32_t] Number of bytes used in buffer
     */
    uint32_t CheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index);

    /**
     * Push a serialized table to the supplied lua state, will increase the stack by 1.
     * * @name PushTable
     * @param L [type: lua_State*] Lua state
     * @param data [type: const char*] Buffer with serialized table to push
     * @param data_size [type: uint32_t] Size of buffer of serialized data
     */
    void PushTable(lua_State* L, const char* data, uint32_t data_size);
}

#endif // DMSDK_SCRIPT_SCRIPT_H
