#ifndef DM_SCRIPT_H
#define DM_SCRIPT_H

#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <ddf/ddf.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    typedef struct Context* HContext;

    /**
     * Create and return a new context.
     * @return context
     */
    HContext NewContext();

    /**
     * Delete an existing context.
     */
    void DeleteContext(HContext context);

    /**
     * Registers a ddf type so it can be used as message when posting in scripts.
     * @param descriptor DDF descriptor of the type
     * @return wether the type could be registered
     */
    bool RegisterDDFType(HContext context, const dmDDF::Descriptor* descriptor);

    /**
     * Callback used to resolve paths.
     * Implementations of this callback are expected to resolve the path given the lua state.
     * @param L lua state
     * @param path
     * @param path_size
     * @return hashed resolved path
     */
    typedef dmhash_t (*ResolvePathCallback)(lua_State* L, const char* path, uint32_t path_size);

    /**
     * Callback used to retrieve url
     * Implementations of this callback are expected to fill out the url appropriately given the lua state.
     * @param L lua state
     * @param out_url Pointer to the url that should be filled with information
     */
    typedef void (*GetURLCallback)(lua_State* L, dmMessage::URL* out_url);

    /**
     * Callback used to retrieve message user data
     * Implementations of this callback are expected to return appropriate user data given the lua state.
     * @param L lua state
     * @return User data pointer
     */
    typedef uintptr_t (*GetUserDataCallback)(lua_State* L);

    /**
     * Parameters to initialize the script context
     */
    struct ScriptParams
    {
        ScriptParams();

        HContext m_Context;
        ResolvePathCallback m_ResolvePathCallback;
        GetURLCallback m_GetURLCallback;
        GetUserDataCallback m_GetUserDataCallback;
    };

    /**
     * Register the script libraries into the supplied lua state.
     * @param L Lua state
     */
    void Initialize(lua_State* L, const ScriptParams& params);

    /**
     * Retrieve a ddf structure from a lua state.
     * @param L Lua state
     * @param descriptor DDF descriptor
     * @param buffer Buffer that will be written to
     * @param buffer_size Buffer size
     * @param index Index of the table
     * @return Number of bytes used in the buffer
     */
    uint32_t CheckDDF(lua_State* L, const dmDDF::Descriptor* descriptor, char* buffer, uint32_t buffer_size, int index);

    /**
     * Push DDF message to Lua stack
     * @param L Lua state
     * @param descriptor Field descriptor
     * @param data DDF data
     */
    void PushDDF(lua_State*L, const dmDDF::Descriptor* descriptor, const char* data);

    /**
     * Serialize a table to a buffer
     * Supported types: LUA_TBOOLEAN, LUA_TNUMBER, LUA_TSTRING, Point3, Vector3, Vector4 and Quat
     * Keys must be strings
     * @param L Lua state
     * @param buffer Buffer that will be written to
     * @param buffer_size Buffer size
     * @param index Index of the table
     * @return Number of bytes used in buffer
     */
    uint32_t CheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index);

    /**
     * Push a serialized table to the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param data Buffer with serialized table to push
     */
    void PushTable(lua_State*L, const char* data);

    /**
     * Check if the value at #index is a hash
     * @param L Lua state
     * @param index Index of the value
     * @return true if the value at #index is a hash
     */
    bool IsHash(lua_State *L, int index);

    /**
     * Push a hash value onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param hash Hash value to push
     */
    void PushHash(lua_State* L, dmhash_t hash);

    /**
     * Check if the value in the supplied index on the lua stack is a hash.
     * @param L Lua state
     * @param index Index of the value
     * @return The hash value
     */
    dmhash_t CheckHash(lua_State* L, int index);

    /**
     * Check if the value at #index is a vector3
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a vector3
     */
    bool IsVector3(lua_State *L, int index);

    /**
     * Push a Vector3 value onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param v Vector3 value to push
     */
    void PushVector3(lua_State* L, const Vectormath::Aos::Vector3& v);

    /**
     * Check if the value in the supplied index on the lua stack is a Vector3.
     * @param L Lua state
     * @param index Index of the value
     * @return The Vector3 value
     */
    Vectormath::Aos::Vector3* CheckVector3(lua_State* L, int index);

    /**
     * Check if the value at #index is a vector4
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a vector4
     */
    bool IsVector4(lua_State *L, int index);

    /**
     * Push a Vector4 value onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param v Vector4 value to push
     */
    void PushVector4(lua_State* L, const Vectormath::Aos::Vector4& v);

    /**
     * Check if the value in the supplied index on the lua stack is a Vector4.
     * @param L Lua state
     * @param index Index of the value
     * @return The Vector4 value
     */
    Vectormath::Aos::Vector4* CheckVector4(lua_State* L, int index);

    /**
     * Check if the value at #index is a quat
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a quat
     */
    bool IsQuat(lua_State *L, int index);

    /**
     * Push a quaternion value onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param q Quaternion value to push
     */
    void PushQuat(lua_State* L, const Vectormath::Aos::Quat& q);

    /**
     * Check if the value in the supplied index on the lua stack is a quaternion.
     * @param L Lua state
     * @param index Index of the value
     * @return The quat value
     */
    Vectormath::Aos::Quat* CheckQuat(lua_State* L, int index);

    /**
     * Check if the value at #index is a matrix4
     * @param L Lua state
     * @param index Index of the value
     * @return true if value at #index is a matrix4
     */
    bool IsMatrix4(lua_State *L, int index);

    /**
     * Push a matrix4 value onto the supplied lua state, will increase the stack by 1.
     * @param L Lua state
     * @param m Matrix4 value to push
     */
    void PushMatrix4(lua_State* L, const Vectormath::Aos::Matrix4& m);

    /**
     * Check if the value in the supplied index on the lua stack is a matrix4.
     * @param L Lua state
     * @param index Index of the value
     * @return The matrix4 value
     */
    Vectormath::Aos::Matrix4* CheckMatrix4(lua_State* L, int index);

    /**
     * Check if the value at #index is a URL
     * @param L Lua state
     * @param index Index of the value
     * @return true is value at #index is a URL
     */
    bool IsURL(lua_State *L, int index);

    /**
     * Push a URL value onto the supplied lua state, will increase the stack by 1
     * @param L Lua state
     * @param a URL value to push
     */
    void PushURL(lua_State* L, const dmMessage::URL& m);

    /**
     * Check if the value in the supplied index on the lua stack is a URL
     * @param L Lua state
     * @param index Index of the value
     * @return The URL value
     */
    dmMessage::URL* CheckURL(lua_State* L, int index);

    /**
     * Returns the URL of a script currently operating on the given lua state.
     * @param L Lua state
     * @param Pointer to a URL to be written to
     * @return true if a URL could be found
     */
    bool GetURL(lua_State* L, dmMessage::URL* out_url);

    /**
     * Returns the user data associated with the script currently operating on the given lua state.
     * @param L Lua state
     * @param Pointer to the pointer to be written to
     * @return true if user data could be found
     */
    bool GetUserData(lua_State* L, uintptr_t* out_user_data);
}

#endif // DM_SCRIPT_H
