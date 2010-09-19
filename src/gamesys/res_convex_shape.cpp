#include "res_convex_shape.h"

#include <dlib/log.h>

#include <physics/physics.h>

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResConvexShapeCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename)
    {
        dmPhysicsDDF::ConvexShape* convex_shape;
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::ConvexShape>(buffer, buffer_size, &convex_shape);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        switch (convex_shape->m_ShapeType)
        {
        case dmPhysicsDDF::ConvexShape::TYPE_SPHERE:
            if (convex_shape->m_Data.m_Count != 1)
            {
                dmLogError("Invalid sphere shape");
                return dmResource::CREATE_RESULT_FORMAT_ERROR;
            }
            resource->m_Resource = dmPhysics::NewSphereShape(convex_shape->m_Data[0]);
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_BOX:
            if (convex_shape->m_Data.m_Count != 3)
            {
                dmLogError("Invalid box shape");
                return dmResource::CREATE_RESULT_FORMAT_ERROR;
            }
            resource->m_Resource = dmPhysics::NewBoxShape(Vectormath::Aos::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_CAPSULE:
            if (convex_shape->m_Data.m_Count != 2)
            {
                dmLogError("Invalid capsule shape");
                return dmResource::CREATE_RESULT_FORMAT_ERROR;
            }
            resource->m_Resource = dmPhysics::NewCapsuleShape(convex_shape->m_Data[0], convex_shape->m_Data[1]);
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_HULL:
            if (convex_shape->m_Data.m_Count < 9)
            {
                dmLogError("Invalid hull shape");
                return dmResource::CREATE_RESULT_FORMAT_ERROR;
            }
            resource->m_Resource = dmPhysics::NewConvexHullShape(&convex_shape->m_Data[0], convex_shape->m_Data.m_Count);
            break;
        }

        dmDDF::FreeMessage(convex_shape);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResConvexShapeDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource)
    {
        dmPhysics::DeleteCollisionShape((dmPhysics::HCollisionShape) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }
}
