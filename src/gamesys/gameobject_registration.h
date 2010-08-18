#ifndef GAMEOBJECT_REGISTRATION_H
#define GAMEOBJECT_REGISTRATION_H

#include <gameobject/gameobject.h>
#include <physics/physics.h>
#include <render/render.h>

namespace dmGameSystem
{
    dmGameObject::Result RegisterModelComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderWorld renderworld);

    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmPhysics::HWorld physics_world);
}

#endif // GAMEOBJECT_REGISTRATION_H
