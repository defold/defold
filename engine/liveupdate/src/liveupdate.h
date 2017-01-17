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

    void HashToString(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* hash, char* buf, uint32_t buflen);

};

#endif // DM_LIVEUPDATE_H