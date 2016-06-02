
#include <script/script.h>

namespace dmFacebook {

    /**
     * Helper function to escape "escapable character sequences" in a string.
     * Used by LuaDialogParamsToJson to escape strings and keys in the JSON table.
     * @param unescaped String to escape
     * @param escaped A user allocated char buffer where the escaped string will be stored.
     * @param end_ptr Pointer to the end of the escaped buffer
     * @return Escaped string length
     */
    int EscapeJsonString(const char* unescaped, char* escaped, char* end_ptr);

    /**
     * Converts a Lua table into a comma seperated string. The table will be treated as an
     * array, ie. keys will not be handled.
     * @param L Lua state
     * @param index Stack location of Lua table
     * @param buffer Output char buffer
     * @param buffer_size Output buffer size
     * @return Length of output string
     */
    size_t LuaStringCommaArray(lua_State* L, int index, char* buffer, size_t buffer_size);

    /**
     * Converts a specifically formatted Lua table into a JSON object, encoded as a string.
     * The Lua table, and the JSON output, is specifically formatted to be used with the
     * dialog functionality in the Android and JS Facebook SDKs.
     * @param L Lua state
     * @param index Stack location of Lua table
     * @param json_buffer User allocated char buffer where the JSON string should be stored.
     * @param json_buffer_size Size of the JSON buffer.
     * @return 1 on success, 0 on failure
     */
    // int LuaDialogParamsToJson(lua_State* L, int index, char* json_buffer, size_t json_buffer_size);

    int WriteEscapedJsonString(char* json_buffer, size_t json_buffer_size, const char* unescaped_value, size_t unescaped_value_len);
    int LuaValueToJson(lua_State* L, int index, char* buffer, size_t buffer_size);

    int DuplicateLuaTable(lua_State* L, int from_index, int to_index, int recursion);
    int DialogTableToEmscripten(lua_State* L, const char* dialog_type, int from_index, int to_index);

    /**
     * Check if a Lua table can be considered an array.
     * If the supplied table only has number keys in ascending order.
     * @param L Lua state
     * @param index Stack location of Lua table
     * @return 1 if table can be considered an array, 0 otherwise
     */
    bool IsLuaArray(lua_State* L, int index);


} // #ifndef DM_FACEBOOK_H
