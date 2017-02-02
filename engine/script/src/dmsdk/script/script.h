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
