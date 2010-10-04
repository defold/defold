#include "res_collision_object.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResCollisionObjectCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        dmPhysicsDDF::CollisionObjectDesc* collision_object_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::CollisionObjectDesc>(buffer, buffer_size, &collision_object_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmPhysics::HCollisionShape collision_shape;
        dmResource::FactoryResult FACTORY_RESULT;
        FACTORY_RESULT = dmResource::Get(factory, collision_object_desc->m_CollisionShape, (void**) &collision_shape);
        if (FACTORY_RESULT != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage(collision_object_desc);
            return dmResource::CREATE_RESULT_UNKNOWN; // TODO: Translate error... we need a new function...
        }

        CollisionObjectPrototype* collision_object_prototype = new CollisionObjectPrototype();
        collision_object_prototype->m_CollisionShape = collision_shape;
        collision_object_prototype->m_Type = (dmPhysics::CollisionObjectType)collision_object_desc->m_Type;
        collision_object_prototype->m_Mass = collision_object_desc->m_Mass;
        collision_object_prototype->m_Friction = collision_object_desc->m_Friction;
        collision_object_prototype->m_Restitution = collision_object_desc->m_Restitution;
        collision_object_prototype->m_Group = (uint16_t)collision_object_desc->m_Group;
        collision_object_prototype->m_Mask = 0;
        for (uint32_t i = 0; i < collision_object_desc->m_Mask.m_Count; ++i)
            collision_object_prototype->m_Mask |= collision_object_desc->m_Mask[i];
        resource->m_Resource = (void*) collision_object_prototype;

        dmDDF::FreeMessage(collision_object_desc);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCollisionObjectDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        CollisionObjectPrototype* collision_object_prototype = (CollisionObjectPrototype*)resource->m_Resource;
        dmResource::Release(factory, collision_object_prototype->m_CollisionShape);
        delete collision_object_prototype;
        return dmResource::CREATE_RESULT_OK;
    }
}
