#include "res_collision_object.h"

#include <string.h>

#include <dlib/hash.h>
#include <dlib/log.h>

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
        CollisionObjectResource* resource, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::CollisionObjectDesc>(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }
        resource->m_Group = dmHashString64(resource->m_DDF->m_Group);
        uint32_t mask_count = resource->m_DDF->m_Mask.m_Count;
        if (mask_count > 16)
        {
            dmLogWarning("The collision object '%s' has a collision mask containing more than 16 groups, the rest will be ignored.", filename);
            mask_count = 16;
        }
        for (uint32_t i = 0; i < mask_count; ++i)
        {
            resource->m_Mask[i] = dmHashString64(resource->m_DDF->m_Mask[i]);
        }

        void* res;
        dmResource::FactoryResult factory_result = dmResource::Get(factory, resource->m_DDF->m_CollisionShape, &res);
        if (factory_result == dmResource::FACTORY_RESULT_OK)
        {
            uint32_t tile_grid_type;
            factory_result = dmResource::GetTypeFromExtension(factory, "tilegridc", &tile_grid_type);
            if (factory_result == dmResource::FACTORY_RESULT_OK)
            {
                uint32_t res_type;
                factory_result = dmResource::GetType(factory, res, &res_type);
                if (factory_result == dmResource::FACTORY_RESULT_OK && res_type == tile_grid_type)
                {
                    resource->m_TileGridResource = (TileGridResource*)res;
                    resource->m_TileGrid = 1;
                    return true;
                }
            }
            resource->m_ConvexShapeResource = (ConvexShapeResource*)res;
            return true;
        }
        else
        {
            return false;
        }
    }

    void ReleaseResources(dmResource::HFactory factory, CollisionObjectResource* resource)
    {
        if (resource->m_TileGrid)
        {
            if (resource->m_TileGridResource)
                dmResource::Release(factory, resource->m_TileGridResource);
        }
        else
        {
            if (resource->m_ConvexShapeResource)
                dmResource::Release(factory, resource->m_ConvexShapeResource);
        }
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
        memset(collision_object, 0, sizeof(CollisionObjectResource));
        if (AcquireResources(factory, buffer, buffer_size, collision_object, filename))
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
        memset(&tmp_collision_object, 0, sizeof(CollisionObjectResource));
        if (AcquireResources(factory, buffer, buffer_size, &tmp_collision_object, filename))
        {
            ReleaseResources(factory, collision_object);
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
