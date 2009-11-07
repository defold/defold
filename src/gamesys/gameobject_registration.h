#ifndef GAMEOBJECT_REGISTRATION_H
#define GAMEOBJECT_REGISTRATION_H

#include "gameobject.h"
#include <physics/physics.h>

namespace dmGameSystem
{
    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HCollection collection,
                                                  dmPhysics::HWorld physics_world);
}

#endif // GAMEOBJECT_REGISTRATION_H
