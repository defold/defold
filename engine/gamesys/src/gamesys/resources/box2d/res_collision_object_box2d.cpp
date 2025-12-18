// Copyright 2020-2025 The Defold Foundation
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

#include "res_collision_object_box2d.h"

#include <string.h> // memset

#include <dlib/hash.h>
#include <dlib/log.h>

#include "../../gamesys.h"
#include "../../resources/res_tilegrid.h"

namespace dmGameSystem
{
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

    bool AcquireResources(PhysicsContextBox2D* physics_context, dmResource::HFactory factory,
        const void* buffer, uint32_t buffer_size, CollisionObjectResourceBox2D* resource, const char* filename)
    {
        CollisionObjectResource* resource_base = (CollisionObjectResource*) resource;

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

        if (resource_base->m_DDF->m_CollisionShape && resource_base->m_DDF->m_CollisionShape[0] != '\0')
        {
            void* res;
            dmResource::Result factory_result = dmResource::Get(factory, resource_base->m_DDF->m_CollisionShape, &res);
            if (factory_result == dmResource::RESULT_OK)
            {
                HResourceType tile_grid_type;
                factory_result = dmResource::GetTypeFromExtension(factory, "tilemapc", &tile_grid_type);
                if (factory_result == dmResource::RESULT_OK)
                {
                    HResourceType res_type;
                    factory_result = dmResource::GetType(factory, res, &res_type);
                    if (factory_result == dmResource::RESULT_OK && res_type == tile_grid_type)
                    {
                        resource->m_TileGridResource = (TileGridResource*)res;
                        resource->m_TileGrid = 1;

                        dmArray<dmPhysics::HCollisionShape2D>& shapes = resource->m_TileGridResource->m_GridShapes;
                        uint32_t shape_count = shapes.Size();
                        uint32_t total_shapes_count = embedded_shape_count + shape_count;
                        resource->m_Shapes2D = (dmPhysics::HCollisionShape2D*)malloc(sizeof(dmPhysics::HCollisionShape2D) * total_shapes_count);
                        resource_base->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * total_shapes_count);
                        resource_base->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * total_shapes_count);
                        for (uint32_t i = 0; i < shape_count; ++i)
                        {
                            resource->m_Shapes2D[i] = resource->m_TileGridResource->m_GridShapes[i];
                            resource_base->m_ShapeTranslation[i] = dmVMath::Vector3(0.0, 0.0, 0.0);
                            resource_base->m_ShapeRotation[i] = dmVMath::Quat(0.0, 0.0, 0.0, 0.0);
                        }
                        resource->m_TileGridShapeCount = shape_count;
                        resource_base->m_ShapeCount = shape_count;
                    }
                }
            }
        }

        if (embedded_shapes)
        {
            if (!resource->m_TileGrid)
            {
                resource->m_Shapes2D = (dmPhysics::HCollisionShape2D*)malloc(sizeof(dmPhysics::HCollisionShape2D) * embedded_shape_count);
                resource_base->m_ShapeTranslation = (dmVMath::Vector3*)malloc(sizeof(dmVMath::Vector3) * embedded_shape_count);
                resource_base->m_ShapeRotation = (dmVMath::Quat*)malloc(sizeof(dmVMath::Quat) * embedded_shape_count);
                resource_base->m_ShapeTypes = (dmPhysicsDDF::CollisionShape::Type*)malloc(sizeof(dmPhysicsDDF::CollisionShape::Type) * embedded_shape_count);
            }

            // Create embedded convex shapes
            uint32_t current_shape_count = resource_base->m_ShapeCount;
            for (uint32_t i = 0; i < embedded_shape_count; ++i)
            {
                dmPhysics::HCollisionObject2D shape = Create2DShape(physics_context->m_Context, embedded_shape, i);
                if (shape)
                {
                    resource->m_Shapes2D[current_shape_count] = shape;
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

    void ReleaseResources(PhysicsContextBox2D* physics_context, dmResource::HFactory factory, CollisionObjectResourceBox2D* resource)
    {
        CollisionObjectResource* resource_base = (CollisionObjectResource*) resource;

        if (resource->m_TileGrid)
        {
            if (resource->m_TileGridResource)
                dmResource::Release(factory, resource->m_TileGridResource);
        }

        if (resource->m_Shapes2D)
        {
            uint32_t shape_count = resource_base->m_ShapeCount;
            for (uint32_t i = resource->m_TileGridShapeCount; i < shape_count; ++i)
            {
                dmPhysics::DeleteCollisionShape2D(resource->m_Shapes2D[i]);
            }
            free(resource->m_Shapes2D);
            free(resource_base->m_ShapeTranslation);
            free(resource_base->m_ShapeRotation);
            free(resource_base->m_ShapeTypes);
        }
        if (resource_base->m_DDF)
        {
            dmDDF::FreeMessage(resource_base->m_DDF);
        }
    }

    dmResource::Result ResCollisionObjectBox2DCreate(const dmResource::ResourceCreateParams* params)
    {
        CollisionObjectResourceBox2D* collision_object = new CollisionObjectResourceBox2D();
        memset(collision_object, 0, sizeof(CollisionObjectResourceBox2D));

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;
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
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollisionObjectBox2DDestroy(const dmResource::ResourceDestroyParams* params)
    {
        CollisionObjectResourceBox2D* collision_object = (CollisionObjectResourceBox2D*) dmResource::GetResource(params->m_Resource);
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;
        ReleaseResources(physics_context, params->m_Factory, collision_object);
        delete collision_object;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollisionObjectBox2DRecreate(const dmResource::ResourceRecreateParams* params)
    {
        CollisionObjectResourceBox2D* collision_object = (CollisionObjectResourceBox2D*)dmResource::GetResource(params->m_Resource);
        CollisionObjectResourceBox2D tmp_collision_object;
        memset(&tmp_collision_object, 0, sizeof(CollisionObjectResourceBox2D));
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;
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
