#ifndef DM_GAMESYS_SCRIPT_GFX_H
#define DM_GAMESYS_SCRIPT_GFX_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    void ScriptGfxRegister(const ScriptLibContext& context);
    void ScriptGfxFinalize(const ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_GFX_H
