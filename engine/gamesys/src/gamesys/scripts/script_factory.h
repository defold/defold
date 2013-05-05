#ifndef DM_GAMESYS_SCRIPT_FACTORY_H
#define DM_GAMESYS_SCRIPT_FACTORY_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    void ScriptFactoryRegister(const ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_FACTORY_H
