#include "res_collision_object.h"

#include <string.h>

namespace dmGameSystem
{
    static const uint32_t MAX_PATH_LENGTH = 128;

    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
        CollisionObjectResource* resource, const char* previous_shape_path)
    {
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::CollisionObjectDesc>(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        bool result = true;

        resource->m_Mask = 0;
        for (uint32_t i = 0; i < resource->m_DDF->m_Mask.m_Count; ++i)
            resource->m_Mask |= resource->m_DDF->m_Mask[i];

        bool reload_shape = true;
        if (previous_shape_path != 0x0)
        {
            reload_shape = 0 != strncmp(previous_shape_path, resource->m_DDF->m_CollisionShape, MAX_PATH_LENGTH);
        }
        if (reload_shape)
        {
            dmResource::FactoryResult factory_result = dmResource::Get(factory, resource->m_DDF->m_CollisionShape, (void**) &resource->m_ConvexShape);
            if (factory_result != dmResource::FACTORY_RESULT_OK)
                result = false;
        }

        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, CollisionObjectResource* resource)
    {
        if (resource->m_ConvexShape)
            dmResource::Release(factory, resource->m_ConvexShape);
        if (resource->m_DDF)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::CreateResult ResCollisionObjectCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        CollisionObjectResource* collision_object = new CollisionObjectResource();
        collision_object->m_DDF = 0x0;
        collision_object->m_ConvexShape = 0x0;
        if (AcquireResources(factory, buffer, buffer_size, collision_object, 0x0))
        {
            resource->m_Resource = collision_object;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, collision_object);
            delete collision_object;
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
    }

    dmResource::CreateResult ResCollisionObjectDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        CollisionObjectResource* collision_object = (CollisionObjectResource*)resource->m_Resource;
        ReleaseResources(factory, collision_object);
        delete collision_object;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCollisionObjectRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        CollisionObjectResource* collision_object = (CollisionObjectResource*)resource->m_Resource;
        CollisionObjectResource tmp_collision_object;
        tmp_collision_object.m_DDF = 0x0;
        tmp_collision_object.m_ConvexShape = 0x0;
        if (AcquireResources(factory, buffer, buffer_size, &tmp_collision_object, collision_object->m_DDF->m_CollisionShape))
        {
            if (tmp_collision_object.m_ConvexShape)
                dmResource::Release(factory, collision_object->m_ConvexShape);
            else
                tmp_collision_object.m_ConvexShape = collision_object->m_ConvexShape;
            if (collision_object->m_DDF)
                dmDDF::FreeMessage(collision_object->m_DDF);
            *collision_object = tmp_collision_object;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_collision_object);
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
    }
}
