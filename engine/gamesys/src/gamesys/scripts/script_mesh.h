#ifndef DM_GAMESYS_SCRIPT_MESH_H
#define DM_GAMESYS_SCRIPT_MESH_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    void ScriptMeshRegister(const ScriptLibContext& context);
    void ScriptMeshFinalize(const ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_MESH_H
