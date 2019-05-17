#ifndef DM_PUSH_UTILS
#define DM_PUSH_UTILS

#include <script/script.h>

#define DM_PUSH_EXTENSION_ORIGIN_REMOTE 0
#define DM_PUSH_EXTENSION_ORIGIN_LOCAL  1

namespace dmPush
{
    bool VerifyPayload(lua_State* L, const char* payload, char* error_str_out, size_t error_str_size);
}

#endif
