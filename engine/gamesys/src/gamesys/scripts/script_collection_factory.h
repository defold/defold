#ifndef DM_GAMESYS_SCRIPT_COLLECTION_FACTORY_H
#define DM_GAMESYS_SCRIPT_COLLECTION_FACTORY_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    void ScriptCollectionFactoryRegister(const ScriptLibContext& context);
}

#endif
