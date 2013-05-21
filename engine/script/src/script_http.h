#ifndef DM_SCRIPT_HTTP_H
#define DM_SCRIPT_HTTP_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmScript
{
    void InitializeHttp(lua_State* L);
    void FinalizeHttp(lua_State* L);
}

#endif // DM_SCRIPT_HTTP_H
