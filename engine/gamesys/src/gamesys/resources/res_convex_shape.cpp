#include "res_convex_shape.h"

#include <dlib/log.h>

#include "gamesys.h"
#include "../proto/physics_ddf.h"

/*
 * NOTE: Convex-shape is obsolete and the code below is not currently in use
 * We currently embed external convex shape resources into the collisionshape at compile-time
 */

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
                           PhysicsContext* context,
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
                if (context->m_3D)
                    resource->m_Shape3D = dmPhysics::NewSphereShape3D(context->m_Context3D, convex_shape->m_Data[0]);
                else
                    resource->m_Shape2D = dmPhysics::NewCircleShape2D(context->m_Context2D, convex_shape->m_Data[0]);
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
                if (context->m_3D)
                    resource->m_Shape3D = dmPhysics::NewBoxShape3D(context->m_Context3D, Vectormath::Aos::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
                else
                    resource->m_Shape2D = dmPhysics::NewBoxShape2D(context->m_Context2D, Vectormath::Aos::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
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
                if (context->m_3D)
                    resource->m_Shape3D = dmPhysics::NewCapsuleShape3D(context->m_Context3D, convex_shape->m_Data[0], convex_shape->m_Data[1]);
                else
                    // TODO: Add support
                    dmLogError("%s", "Capsules are not supported in 2D.");
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
                if (context->m_3D)
                    resource->m_Shape3D = dmPhysics::NewConvexHullShape3D(context->m_Context3D, &convex_shape->m_Data[0], convex_shape->m_Data.m_Count / 3);
                else
                {
                    const uint32_t data_size = 2 * convex_shape->m_Data.m_Count / 3;
                    float* data_2d = new float[2 * convex_shape->m_Data.m_Count / 3];
                    for (uint32_t i = 0; i < data_size; ++i)
                    {
                        data_2d[i] = convex_shape->m_Data[i/2*3 + i%2];
                    }
                    resource->m_Shape2D = dmPhysics::NewPolygonShape2D(context->m_Context2D, data_2d, data_size/2);
                    delete [] data_2d;
                }
            }
            break;
        }

        dmDDF::FreeMessage(convex_shape);
        return result;
    }

    dmResource::Result ResConvexShapeCreate(const dmResource::ResourceCreateParams& params)
    {
        ConvexShapeResource* convex_shape = new ConvexShapeResource();
        convex_shape->m_3D = ((PhysicsContext*) params.m_Context)->m_3D;
        if (AcquireResources(params.m_Factory, (PhysicsContext*) params.m_Context, params.m_Buffer, params.m_BufferSize, convex_shape, params.m_Filename))
        {
            params.m_Resource->m_Resource = convex_shape;
            return dmResource::RESULT_OK;
        }
        else
        {
            delete convex_shape;
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    void ReleaseResources(ConvexShapeResource* resource)
    {
        if (resource->m_Shape3D)
        {
            if (resource->m_3D)
                dmPhysics::DeleteCollisionShape3D(resource->m_Shape3D);
            else
                dmPhysics::DeleteCollisionShape2D(resource->m_Shape2D);
        }
    }

    dmResource::Result ResConvexShapeDestroy(const dmResource::ResourceDestroyParams& params)
    {
        ConvexShapeResource* convex_shape = (ConvexShapeResource*)params.m_Resource->m_Resource;
        ReleaseResources(convex_shape);
        delete convex_shape;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResConvexShapeRecreate(const dmResource::ResourceRecreateParams& params)
    {
        ConvexShapeResource* cs_resource = (ConvexShapeResource*)params.m_Resource->m_Resource;
        ConvexShapeResource tmp_convex_shape;
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        tmp_convex_shape.m_3D = physics_context->m_3D;
        if (AcquireResources(params.m_Factory, (PhysicsContext*) params.m_Context, params.m_Buffer, params.m_BufferSize, &tmp_convex_shape, params.m_Filename))
        {
            if (physics_context->m_3D)
                dmPhysics::ReplaceShape3D(physics_context->m_Context3D, cs_resource->m_Shape3D, tmp_convex_shape.m_Shape3D);
            else
                dmPhysics::ReplaceShape2D(physics_context->m_Context2D, cs_resource->m_Shape2D, tmp_convex_shape.m_Shape2D);
            ReleaseResources(cs_resource);
            cs_resource->m_Shape3D = tmp_convex_shape.m_Shape3D;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
