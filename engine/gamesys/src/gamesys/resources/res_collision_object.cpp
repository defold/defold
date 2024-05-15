// Copyright 2020-2024 The Defold Foundation
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

#include "res_collision_object.h"

#include <string.h> // memset

#include <dlib/hash.h>
#include <dlib/log.h>
#include "../gamesys.h"
#include "../resources/res_tilegrid.h"

namespace dmGameSystem
{
    CollisionObjectResource::CollisionObjectResource()
    {
        memset(this, 0, sizeof(CollisionObjectResource));
    }

    static dmPhysics::HCollisionShape2D Create2DShape(dmPhysics::HContext2D context, const dmPhysicsDDF::CollisionShape* collision_shape, uint32_t shape_index)
    {
        const dmPhysicsDDF::CollisionShape::Shape* shape = &collision_shape->m_Shapes[shape_index];

        const float* data = collision_shape->m_Data.m_Data;
        uint32_t data_count = collision_shape->m_Data.m_Count;
        dmPhysics::HCollisionShape2D ret = 0;

        switch (shape->m_ShapeType)
        {
        case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            if (shape->m_Index + 1 > data_count)
            {
                goto range_error;
            }
            ret = dmPhysics::NewCircleShape2D(context, data[shape->m_Index]);
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            if (shape->m_Index + 3 > data_count)
            {
                goto range_error;
            }
            ret = dmPhysics::NewBoxShape2D(context, dmVMath::Vector3(data[shape->m_Index], data[shape->m_Index+1], data[shape->m_Index+2]));
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
            // TODO: Add support
            dmLogError("%s", "Capsules are not supported in 2D.");
            break;

        case dmPhysicsDDF::CollisionShape::TYPE_HULL:
        {
            if (shape->m_Index + shape->m_Count > data_count)
            {
                goto range_error;
            }

            const uint32_t data_size = 2 * shape->m_Count / 3;
            float* data_2d = new float[2 * shape->m_Count / 3];
            for (uint32_t i = 0; i < data_size; ++i)
            {
                data_2d[i] = collision_shape->m_Data[shape->m_Index + i/2*3 + i%2];
            }
            ret = dmPhysics::NewPolygonShape2D(context, data_2d, data_size/2);
            delete [] data_2d;
        }
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

    bool AcquireResources(PhysicsContext* physics_context, dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
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

        const dmPhysicsDDF::CollisionShape* embedded_shape = &resource->m_DDF->m_EmbeddedCollisionShape;
        const dmPhysicsDDF::CollisionShape::Shape* embedded_shapes = embedded_shape->m_Shapes.m_Data;
        const uint32_t embedded_shape_count = embedded_shape->m_Shapes.m_Count;

        if (resource->m_DDF->m_CollisionShape && resource->m_DDF->m_CollisionShape[0] != '\0')
        {
            void* res;
            dmResource::Result factory_result = dmResource::Get(factory, resource->m_DDF->m_CollisionShape, &res);
            if (factory_result == dmResource::RESULT_OK)
            {
                dmResource::ResourceType tile_grid_type;
                factory_result = dmResource::GetTypeFromExtension(factory, "tilemapc", &tile_grid_type);
                if (factory_result == dmResource::RESULT_OK)
                {
                    dmResource::ResourceType res_type;
                    factory_result = dmResource::GetType(factory, res, &res_type);
                    if (factory_result == dmResource::RESULT_OK && res_type == tile_grid_type)
                    {
                        resource->m_TileGridResource = (TileGridResource*)res;
                        resource->m_TileGrid = 1;

                        dmArray<dmPhysics::HCollisionShape2D>& shapes = resource->m_TileGridResource->m_GridShapes;
                        uint32_t shape_count = shapes.Size();
                        uint32_t total_shapes_count = embedded_shape_count + shape_count;
                        resource->m_Shapes2D = (dmPhysics::HCollisionShape2D*)malloc(sizeof(dmPhysics::HCollisionShape2D) * total_shapes_count);
                        resource->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * total_shapes_count);
                        resource->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * total_shapes_count);
                        for (uint32_t i = 0; i < shape_count; ++i)
                        {
                            resource->m_Shapes2D[i] = resource->m_TileGridResource->m_GridShapes[i];
                            resource->m_ShapeTranslation[i] = dmVMath::Vector3(0.0, 0.0, 0.0);
                            resource->m_ShapeRotation[i] = dmVMath::Quat(0.0, 0.0, 0.0, 0.0);
                        }
                        resource->m_TileGridShapeCount = shape_count;
                        resource->m_ShapeCount = shape_count;
                    }
                }
            }
        }

        if (embedded_shapes)
        {
            if (physics_context->m_3D)
            {
                resource->m_Shapes3D = (dmPhysics::HCollisionShape3D*)malloc(sizeof(dmPhysics::HCollisionShape3D) * embedded_shape_count);
                resource->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * embedded_shape_count);
                resource->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * embedded_shape_count);
                resource->m_ShapeTypes = (dmPhysicsDDF::CollisionShape::Type*)malloc(sizeof(dmPhysicsDDF::CollisionShape::Type) * embedded_shape_count);
            }
            else if (!resource->m_TileGrid)
            {
                resource->m_Shapes2D = (dmPhysics::HCollisionShape2D*)malloc(sizeof(dmPhysics::HCollisionShape2D) * embedded_shape_count);
                resource->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * embedded_shape_count);
                resource->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * embedded_shape_count);
                resource->m_ShapeTypes = (dmPhysicsDDF::CollisionShape::Type*)malloc(sizeof(dmPhysicsDDF::CollisionShape::Type) * embedded_shape_count);
            }

            // Create embedded convex shapes
            uint32_t current_shape_count = resource->m_ShapeCount;
            for (uint32_t i = 0; i < embedded_shape_count; ++i)
            {
                if (physics_context->m_3D)
                {
                    dmPhysics::HCollisionObject3D shape = Create3DShape(physics_context->m_Context3D, embedded_shape, i);
                    if (shape)
                    {
                        resource->m_Shapes3D[current_shape_count] = shape;
                        resource->m_ShapeTranslation[current_shape_count] = dmVMath::Vector3(embedded_shapes[i].m_Position);
                        resource->m_ShapeRotation[current_shape_count] = embedded_shapes[i].m_Rotation;
                        resource->m_ShapeTypes[current_shape_count] = embedded_shapes[i].m_ShapeType;
                        current_shape_count++;
                    }
                    else
                    {
                        resource->m_ShapeCount = current_shape_count;
                        return false;
                    }
                }
                else
                {
                    dmPhysics::HCollisionObject2D shape = Create2DShape(physics_context->m_Context2D, embedded_shape, i);
                    if (shape)
                    {
                        resource->m_Shapes2D[current_shape_count] = shape;
                        resource->m_ShapeTranslation[current_shape_count] = dmVMath::Vector3(embedded_shapes[i].m_Position);
                        resource->m_ShapeRotation[current_shape_count] = embedded_shapes[i].m_Rotation;
                        resource->m_ShapeTypes[current_shape_count] = embedded_shapes[i].m_ShapeType;
                        current_shape_count++;
                    }
                    else
                    {
                        resource->m_ShapeCount = current_shape_count;
                        return false;
                    }
                }
            }
            resource->m_ShapeCount = current_shape_count;
        }

        if (resource->m_ShapeCount == 0)
        {
            dmLogError("No shapes found in collision object");
            return false;
        }
        return true;
    }

    void ReleaseResources(PhysicsContext* physics_context, dmResource::HFactory factory, CollisionObjectResource* resource)
    {
        if (resource->m_TileGrid)
        {
            if (resource->m_TileGridResource)
                dmResource::Release(factory, resource->m_TileGridResource);
        }

        uint32_t shape_count = resource->m_ShapeCount;
        if (shape_count > 0)
        {
            for (uint32_t i = resource->m_TileGridShapeCount; i < shape_count; ++i)
            {
                if (physics_context->m_3D)
                {
                    dmPhysics::DeleteCollisionShape3D(resource->m_Shapes3D[i]);
                }
                else
                {
                    dmPhysics::DeleteCollisionShape2D(resource->m_Shapes2D[i]);
                }
            }
            if (physics_context->m_3D)
            {
                free(resource->m_Shapes3D);
            }
            else
            {
                free(resource->m_Shapes2D);
            }
            free(resource->m_ShapeTranslation);
            free(resource->m_ShapeRotation);
        }
        if (resource->m_DDF)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::Result ResCollisionObjectCreate(const dmResource::ResourceCreateParams& params)
    {
        CollisionObjectResource* collision_object = new CollisionObjectResource();
        memset(collision_object, 0, sizeof(CollisionObjectResource));
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (AcquireResources(physics_context, params.m_Factory, params.m_Buffer, params.m_BufferSize, collision_object, params.m_Filename))
        {
            params.m_Resource->m_Resource = collision_object;
            return dmResource::RESULT_OK;
        }
        else
        {
            ReleaseResources(physics_context, params.m_Factory, collision_object);
            delete collision_object;
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResCollisionObjectDestroy(const dmResource::ResourceDestroyParams& params)
    {
        CollisionObjectResource* collision_object = (CollisionObjectResource*)params.m_Resource->m_Resource;
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        ReleaseResources(physics_context, params.m_Factory, collision_object);
        delete collision_object;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollisionObjectRecreate(const dmResource::ResourceRecreateParams& params)
    {
        CollisionObjectResource* collision_object = (CollisionObjectResource*)params.m_Resource->m_Resource;
        CollisionObjectResource tmp_collision_object;
        memset(&tmp_collision_object, 0, sizeof(CollisionObjectResource));
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (AcquireResources(physics_context, params.m_Factory, params.m_Buffer, params.m_BufferSize, &tmp_collision_object, params.m_Filename))
        {
            ReleaseResources(physics_context, params.m_Factory, collision_object);
            *collision_object = tmp_collision_object;
            return dmResource::RESULT_OK;
        }
        else
        {
            ReleaseResources(physics_context, params.m_Factory, &tmp_collision_object);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
