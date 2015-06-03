#ifndef DM_PUSH_UTILS
#define DM_PUSH_UTILS

#include <dlib/json.h>
#include <script/script.h>

int JsonToLua(lua_State* L, dmJson::Document* doc, int index);

#endif