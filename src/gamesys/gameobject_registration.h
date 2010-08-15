#ifndef GAMEOBJECT_REGISTRATION_H
#define GAMEOBJECT_REGISTRATION_H

#include <gameobject/gameobject.h>
#include <physics/physics.h>
#include <render/material_ddf.h>
#include <render/mesh_ddf.h>
#include <render/material_ddf.h>
#include <render/rendercontext.h>
#include <render/render.h>
#include <render/model/model.h>

namespace dmGameSystem
{
    dmGameObject::Result RegisterModelComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmModel::HWorld model_world);

    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmPhysics::HWorld physics_world);
}

#endif // GAMEOBJECT_REGISTRATION_H
