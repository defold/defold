#ifndef RESOURCE_CREATION_H
#define RESOURCE_CREATION_H

#include <physics/physics.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>
#include <render/material_ddf.h>
#include <render/mesh_ddf.h>
#include <render/material_ddf.h>
#include <render/rendercontext.h>
#include <render/render.h>
#include <render/model/model.h>

struct CollisionObjectPrototype
{
    dmPhysics::HCollisionShape m_CollisionShape;
    float m_Mass;
    dmPhysics::CollisionObjectType m_Type;
    uint16_t m_Group;
    uint16_t m_Mask;
};

namespace dmGameSystem
{
    dmResource::FactoryResult RegisterResources(dmResource::HFactory factory);

    dmGameObject::Result RegisterModelComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmModel::HWorld model_world);

}

#endif // RESOURCE_CREATION_H
