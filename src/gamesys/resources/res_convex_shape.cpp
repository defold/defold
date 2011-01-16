#include "res_convex_shape.h"

#include <dlib/log.h>

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
                           const void* buffer, uint32_t buffer_size,
                           ConvexShapeResource* resource,
                           const char* filename)
    {
        dmPhysicsDDF::ConvexShape* convex_shape;
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::ConvexShape>(buffer, buffer_size, &convex_shape);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        bool result = true;
        switch (convex_shape->m_ShapeType)
        {
        case dmPhysicsDDF::ConvexShape::TYPE_SPHERE:
            if (convex_shape->m_Data.m_Count != 1)
            {
                dmLogError("Invalid sphere shape");
                result = false;
            }
            else
            {
                resource->m_Shape = dmPhysics::NewSphereShape(convex_shape->m_Data[0]);
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_BOX:
            if (convex_shape->m_Data.m_Count != 3)
            {
                dmLogError("Invalid box shape");
                result = false;
            }
            else
            {
                resource->m_Shape = dmPhysics::NewBoxShape(Vectormath::Aos::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_CAPSULE:
            if (convex_shape->m_Data.m_Count != 2)
            {
                dmLogError("Invalid capsule shape");
                result = false;
            }
            else
            {
                resource->m_Shape = dmPhysics::NewCapsuleShape(convex_shape->m_Data[0], convex_shape->m_Data[1]);
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_HULL:
            if (convex_shape->m_Data.m_Count < 9)
            {
                dmLogError("Invalid hull shape");
                result = false;
            }
            else
            {
                resource->m_Shape = dmPhysics::NewConvexHullShape(&convex_shape->m_Data[0], convex_shape->m_Data.m_Count);
            }
            break;
        }

        dmDDF::FreeMessage(convex_shape);
        return result;
    }

    dmResource::CreateResult ResConvexShapeCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename)
    {
        ConvexShapeResource* convex_shape = new ConvexShapeResource();
        if (AcquireResources(factory, buffer, buffer_size, convex_shape, filename))
        {
            resource->m_Resource = convex_shape;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            delete convex_shape;
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
    }

    void ReleaseResources(ConvexShapeResource* resource)
    {
        if (resource->m_Shape)
            dmPhysics::DeleteCollisionShape(resource->m_Shape);
    }

    dmResource::CreateResult ResConvexShapeDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource)
    {
        ConvexShapeResource* convex_shape = (ConvexShapeResource*)resource->m_Resource;
        ReleaseResources(convex_shape);
        delete convex_shape;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResConvexShapeRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        ConvexShapeResource* convex_shape = (ConvexShapeResource*)resource->m_Resource;
        ConvexShapeResource tmp_convex_shape;
        if (AcquireResources(factory, buffer, buffer_size, &tmp_convex_shape, filename))
        {
            ReleaseResources(convex_shape);
            convex_shape->m_Shape = tmp_convex_shape.m_Shape;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
    }
}
