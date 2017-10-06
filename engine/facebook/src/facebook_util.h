#ifndef DM_FACEBOOK_UTIL_H
#define DM_FACEBOOK_UTIL_H

#include <script/script.h>

namespace dmFacebook {

    /**
     * Check if a Lua table can be considered an array.
     * If the supplied table only has number keys in ascending order.
     * @param L Lua state
     * @param index Stack location of Lua table
     * @return 1 if table can be considered an array, 0 otherwise
     */
    bool IsLuaArray(lua_State* L, int index);

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
     * Duplicates a Lua table contents into a new table.
     * Only the most basic entry types are handled. number, string and tables for values,
     * and number and strings for keys.
     * @note There is no cyclic reference checking just a max recursion depth.
     * @param L Lua state
     * @param from_index Stack location of input table
     * @param to_index Stack location of output table
     * @param max_recursion_depth Max table recursions allowed
     * @return 1 on success, 0 on failure
     */
    int DuplicateLuaTable(lua_State* L, int from_index, int to_index, unsigned int max_recursion_depth);

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
     * Convert a Lua value into a JSON value, encoded as a non-prettyfied null terminated string.
     * @param L Lua state
     * @param index Stack location of Lua table
     * @param buffer Output JSON char buffer
     * @param buffer_size Output buffer size
     * @return Length of output string, 0 if conversion failed
     */
    int LuaValueToJsonValue(lua_State* L, int index, char* buffer, size_t buffer_size);

    /**
     * Convert a Lua table into a JSON object, encoded as a non-prettyfied null terminated string.
     * If the destination buffer is null, the function will still return the number of characters needed to store the string (minus the null space character).
     * @param L Lua state
     * @param index Stack location of Lua table
     * @param buffer Output JSON char buffer. May be null
     * @param buffer_size Output buffer size
     * @return Length of output string, 0 if conversion failed
     */
    int LuaTableToJson(lua_State* L, int index, char* buffer, size_t buffer_size);

    /**
     * Split a string into a Lua table of strings, splits on the char in 'split'.
     * @param L Lua state
     * @param index Stack location of Lua table to populate.
     * @param str String to split
     * @param split Char to split on.
     */
    void SplitStringToTable(lua_State* L, int index, const char* str, char split);

    /**
     * Escapes "escapable character sequences" in a string and writes it to an output buffer.
     * @param json_buffer Output JSON char buffer
     * @param json_buffer_size Output buffer size
     * @param unescaped_value Unescaped string buffer
     * @param unescaped_value_len Length of unescaped string
     * @return Length of output string, 0 if write failed
     */
    int WriteEscapedJsonString(char* json_buffer, size_t json_buffer_size, const char* unescaped_value, size_t unescaped_value_len);

    /**
     * Converts a Lua table to a platform specific "facebook.show_dialog" param table.
     * The Lua table will be formatted to be used with the internal functionality of
     * the JavaScript Facebook SDK. The formatting tries to unify the field names
     * and value types with the iOS implementation.
     * @param L Lua state
     * @param dialog_type Dialog type that the param table will be used for
     * @param from_index Stack location of input table
     * @param to_index Stack location of output table
     * @return 1 on success, 0 on failure
     */
    int DialogTableToEmscripten(lua_State* L, const char* dialog_type, int from_index, int to_index);

    /**
     * Converts a Lua table to a platform specific "facebook.show_dialog" param table.
     * The Lua table will be formatted to be used with the internal functionality of
     * the Android Facebook SDK. The formatting tries to unify the field names
     * and value types with the iOS implementation.
     * @param L Lua state
     * @param dialog_type Dialog type that the param table will be used for
     * @param from_index Stack location of input table
     * @param to_index Stack location of output table
     * @return 1 on success, 0 on failure
     */
    int DialogTableToAndroid(lua_State* L, const char* dialog_type, int from_index, int to_index);

    /**
     * Count all length of all string values in a Lua table.
     * @param L Lua state
     * @param table_index Stack location of table
     * @param entry_count Count of string entries in table
     * @return Total length of all string values
     */
    size_t CountStringArrayLength(lua_State* L, int table_index, size_t& entry_count);

    void JoinCStringArray(const char** array, uint32_t arrlen, char* buffer, uint32_t buflen, const char* delimiter);

    int luaTableToCArray(lua_State* L, int index, char** buffer, uint32_t buffer_size);

}

#endif // #ifndef DM_FACEBOOK_UTIL_H
