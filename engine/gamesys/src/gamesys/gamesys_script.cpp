#include "gamesys.h"

#include <dlib/log.h>
#include <physics/physics.h>

#include "gamesys_ddf.h"
#include "gamesys.h"

#include "scripts/script_particlefx.h"
#include "scripts/script_tilemap.h"
#include "scripts/script_physics.h"
#include "scripts/script_sprite.h"
#include "scripts/script_factory.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{

    ScriptLibContext::ScriptLibContext()
    {
        memset(this, 0, sizeof(*this));
    }

    bool InitializeScriptLibs(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;

        int top = lua_gettop(L);
        (void)top;

        bool result = true;

        ScriptParticleFXRegister(context);
        ScriptTileMapRegister(context);
        ScriptPhysicsRegister(context);
        ScriptFactoryRegister(context);
        ScriptSpriteRegister(context);

        assert(top == lua_gettop(L));
        return result;
    }

    void FinalizeScriptLibs(const ScriptLibContext& context)
    {
        ScriptPhysicsFinalize(context);
    }

#undef PHYSICS_CONTEXT_NAME
#undef COLLISION_OBJECT_EXT
}
