#ifndef DM_GAMESYS_SCRIPT_BUFFER_H
#define DM_GAMESYS_SCRIPT_BUFFER_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>
#include "../gamesys.h"

namespace dmGameSystem
{
    // void InitializeBuffer(lua_State* L);
    void ScriptBufferRegister(const ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_BUFFER_H
