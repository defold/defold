#include <dlib/dstrings.h>

#include "push_utils.h"

bool dmPush::VerifyPayload(lua_State* L, const char* payload, char* error_str_out, size_t error_str_size)
{
    int top = lua_gettop(L);
    bool success = false;

    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse(payload, &doc);
    if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
        if (dmScript::JsonToLua(L, &doc, 0, error_str_out, error_str_size) >= 0) {
            success = true;
        }
    } else {
        DM_SNPRINTF(error_str_out, error_str_size, "Failed to parse JSON payload string (%d).", r);
        dmJson::Free(&doc);
    }

    assert(top == lua_gettop(L));
    return success;
}
