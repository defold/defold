// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_collision_object_bullet3d.h"

#include <string.h> // memset

#include <dlib/hash.h>
#include <dlib/log.h>

#include "../../gamesys.h"

namespace dmGameSystem
{
    static dmPhysics::HCollisionShape2D Create3DShape(dmPhysics::HContext3D context, const dmPhysicsDDF::CollisionShape* collision_shape, uint32_t shape_index)
    {
        const dmPhysicsDDF::CollisionShape::Shape* shape = &collision_shape->m_Shapes[shape_index];

        const float* data = collision_shape->m_Data.m_Data;
        uint32_t data_count = collision_shape->m_Data.m_Count;
        dmPhysics::HCollisionShape3D ret = 0;

        switch (shape->m_ShapeType)
        {
        case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            if (shape->m_Index + 1 > data_count) {
                goto range_error;
            }
            ret = dmPhysics::NewSphereShape3D(context, data[shape->m_Index]);
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            if (shape->m_Index + 3 > data_count) {
                goto range_error;
            }
            ret = dmPhysics::NewBoxShape3D(context, dmVMath::Vector3(data[shape->m_Index], data[shape->m_Index+1], data[shape->m_Index+2]));
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
            if (shape->m_Index + 2 > data_count) {
                goto range_error;
            }
            ret = dmPhysics::NewCapsuleShape3D(context, data[shape->m_Index], data[shape->m_Index+1]);
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_HULL:
            if (shape->m_Index + shape->m_Count > data_count)
            {
                goto range_error;
            }
            ret = dmPhysics::NewConvexHullShape3D(context, &collision_shape->m_Data[shape->m_Index], shape->m_Count / 3);
            break;

        default:
            // NOTE: We do not create hulls here. Hulls can't currently be created as embedded shapes
            // In the future we might support that or support the collision shape message as a external resource
            dmLogError("Unknown or unsupported shape type: %d", shape->m_ShapeType);
            break;
        }

        return ret;

range_error:
        dmLogError("Index out of range to shape data for shape index %d", shape_index);
        return 0;
    }

    bool AcquireResources(PhysicsContextBullet3D* physics_context, dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
        CollisionObjectResourceBullet3D* resource, const char* filename)
    {
        CollisionObjectResource* resource_base = (CollisionObjectResource*) &resource->m_BaseResource;

        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::CollisionObjectDesc>(buffer, buffer_size, &resource_base->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }
        resource_base->m_Group = dmHashString64(resource_base->m_DDF->m_Group);
        uint32_t mask_count = resource_base->m_DDF->m_Mask.m_Count;
        if (mask_count > 16)
        {
            dmLogWarning("The collision object '%s' has a collision mask containing more than 16 groups, the rest will be ignored.", filename);
            mask_count = 16;
        }
        for (uint32_t i = 0; i < mask_count; ++i)
        {
            resource_base->m_Mask[i] = dmHashString64(resource_base->m_DDF->m_Mask[i]);
        }

        const dmPhysicsDDF::CollisionShape* embedded_shape = &resource_base->m_DDF->m_EmbeddedCollisionShape;
        const dmPhysicsDDF::CollisionShape::Shape* embedded_shapes = embedded_shape->m_Shapes.m_Data;
        const uint32_t embedded_shape_count = embedded_shape->m_Shapes.m_Count;

        if (embedded_shapes)
        {
            resource->m_Shapes3D = (dmPhysics::HCollisionShape3D*)malloc(sizeof(dmPhysics::HCollisionShape3D) * embedded_shape_count);
            resource_base->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * embedded_shape_count);
            resource_base->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * embedded_shape_count);
            resource_base->m_ShapeTypes = (dmPhysicsDDF::CollisionShape::Type*)malloc(sizeof(dmPhysicsDDF::CollisionShape::Type) * embedded_shape_count);

            // Create embedded convex shapes
            uint32_t current_shape_count = resource_base->m_ShapeCount;
            for (uint32_t i = 0; i < embedded_shape_count; ++i)
            {
                dmPhysics::HCollisionObject3D shape = Create3DShape(physics_context->m_Context, embedded_shape, i);
                if (shape)
                {
                    resource->m_Shapes3D[current_shape_count] = shape;
                    resource_base->m_ShapeTranslation[current_shape_count] = dmVMath::Vector3(embedded_shapes[i].m_Position);
                    resource_base->m_ShapeRotation[current_shape_count] = embedded_shapes[i].m_Rotation;
                    resource_base->m_ShapeTypes[current_shape_count] = embedded_shapes[i].m_ShapeType;
                    current_shape_count++;
                }
                else
                {
                    resource_base->m_ShapeCount = current_shape_count;
                    return false;
                }
            }
            resource_base->m_ShapeCount = current_shape_count;
        }

        if (resource_base->m_ShapeCount == 0)
        {
            dmLogError("No shapes found in collision object");
            return false;
        }
        return true;
    }

    void ReleaseResources(PhysicsContextBullet3D* physics_context, dmResource::HFactory factory, CollisionObjectResourceBullet3D* resource)
    {
        CollisionObjectResource* resource_base = (CollisionObjectResource*) &resource->m_BaseResource;
        uint32_t shape_count = resource_base->m_ShapeCount;
        if (shape_count > 0)
        {
            for (uint32_t i = 0; i < resource_base->m_ShapeCount; ++i)
            {
                dmPhysics::DeleteCollisionShape3D(resource->m_Shapes3D[i]);
            }
            free(resource->m_Shapes3D);
            free(resource_base->m_ShapeTranslation);
            free(resource_base->m_ShapeRotation);
            free(resource_base->m_ShapeTypes);
        }
        if (resource_base->m_DDF)
            dmDDF::FreeMessage(resource_base->m_DDF);
    }

    dmResource::Result ResCollisionObjectBullet3DCreate(const dmResource::ResourceCreateParams* params)
    {
        CollisionObjectResourceBullet3D* collision_object = new CollisionObjectResourceBullet3D();
        memset(collision_object, 0, sizeof(CollisionObjectResource));
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params->m_Context;
        if (AcquireResources(physics_context, params->m_Factory, params->m_Buffer, params->m_BufferSize, collision_object, params->m_Filename))
        {
            dmResource::SetResource(params->m_Resource, collision_object);
            return dmResource::RESULT_OK;
        }
        else
        {
            ReleaseResources(physics_context, params->m_Factory, collision_object);
            delete collision_object;
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResCollisionObjectBullet3DDestroy(const dmResource::ResourceDestroyParams* params)
    {
        CollisionObjectResourceBullet3D* collision_object = (CollisionObjectResourceBullet3D*)dmResource::GetResource(params->m_Resource);
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params->m_Context;
        ReleaseResources(physics_context, params->m_Factory, collision_object);
        delete collision_object;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollisionObjectBullet3DRecreate(const dmResource::ResourceRecreateParams* params)
    {
        CollisionObjectResourceBullet3D* collision_object = (CollisionObjectResourceBullet3D*)dmResource::GetResource(params->m_Resource);
        CollisionObjectResourceBullet3D tmp_collision_object;
        memset(&tmp_collision_object, 0, sizeof(CollisionObjectResourceBullet3D));
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params->m_Context;
        if (AcquireResources(physics_context, params->m_Factory, params->m_Buffer, params->m_BufferSize, &tmp_collision_object, params->m_Filename))
        {
            ReleaseResources(physics_context, params->m_Factory, collision_object);
            *collision_object = tmp_collision_object;
            return dmResource::RESULT_OK;
        }
        else
        {
            ReleaseResources(physics_context, params->m_Factory, &tmp_collision_object);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
