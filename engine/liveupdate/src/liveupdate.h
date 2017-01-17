#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <gamesys/gamesys.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{

    void Initialize(const dmGameSystem::ScriptLibContext& context);
    void Finalize(const dmGameSystem::ScriptLibContext& context);

};

#endif // DM_LIVEUPDATE_H