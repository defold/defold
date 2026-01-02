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

#include "../comp_collision_object.h"
#include "../comp_collision_object_private.h"
#include "comp_collision_object_box2d.h"

#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/static_assert.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/gameobject/script.h>

#include <physics/physics.h>

#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>

#include "gamesys.h"
#include "gamesys_private.h" // ShowFullBufferError

#include "../../resources/box2d/res_collision_object_box2d.h"
#include "../../resources/res_textureset.h"
#include "../../resources/res_tilegrid.h"

#include <gamesys/atlas_ddf.h>
#include <gamesys/texture_set_ddf.h>
#include <gamesys/gamesys_ddf.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_CollisionObjectBox2D, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

    static PhysicsAdapterFunctionTable* g_PhysicsAdapter = 0x0;
    // TODO: Allow the SetWorldTransform to have a physics context which we can check instead!!
    static int g_NumPhysicsTransformsUpdated = 0;
    static bool g_CollisionOverflowWarning   = false;
    static bool g_ContactOverflowWarning     = false;

    struct JointEntry;
    struct JointEndPoint;

    struct CollisionComponentBox2D;
    struct CollisionWorldBox2D;

    static void InstallBox2DPhysicsAdapter();
    static void DeleteJoint(CollisionWorldBox2D* world, dmPhysics::HJoint joint);
    static void DeleteJoint(CollisionWorldBox2D* world, JointEntry* joint_entry);
    static void GetWorldTransform(void* user_data, dmTransform::Transform& world_transform);
    static void SetWorldTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation);

    struct CollisionWorldBox2D
    {
        CollisionWorld                    m_BaseWorld;
        dmPhysics::HWorld2D               m_World2D;
        float                             m_CurrentDT;    // Used to calculate joint reaction force and torque.
        uint8_t                           m_ComponentTypeIndex;
        uint8_t                           m_FirstUpdate : 1;
        dmArray<CollisionComponentBox2D*> m_Components;
    };

    struct CollisionComponentBox2D
    {
        CollisionComponent            m_BaseComponent;
        dmPhysics::HCollisionObject2D m_Object2D;
        /// Linked list of joints FROM this component.
        JointEntry*                   m_Joints;
        /// Linked list of joints TO this component.
        JointEndPoint*                m_JointEndPoints;
        uint8_t                       m_FlippedX : 1; // set if it's been flipped
        uint8_t                       m_FlippedY : 1; // --||--
        uint8_t                                  : 6; // Unused
    };

    /// Joint entry that will keep track of joint connections from collision components.
    struct JointEntry
    {
        dmhash_t             m_Id;
        dmPhysics::JointType m_Type;
        dmPhysics::HJoint    m_Joint;
        JointEntry*          m_Next;
        JointEndPoint*       m_EndPoint;

        JointEntry(dmhash_t id, dmPhysics::HJoint joint, JointEntry* next)
        {
            m_Id = id;
            m_Joint = joint;
            m_Next = next;
        }
    };

    /// Joint end point to keep track of joint connections to collision components.
    struct JointEndPoint
    {
        JointEndPoint*           m_Next;
        CollisionComponentBox2D* m_Owner;
        JointEntry*              m_JointEntry;
    };

    struct RayCastUserData
    {
        dmGameObject::HInstance m_Instance;
        CollisionWorldBox2D*    m_World;
    };

    struct DispatchContext
    {
        PhysicsContextBox2D*    m_PhysicsContext;
        dmGameObject::HRegister m_Register;
        uint32_t                m_ComponentTypeIndex;
        bool                    m_Success;
    };

    dmGameObject::CreateResult CompCollisionObjectBox2DNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        DM_STATIC_ASSERT(sizeof(ShapeInfo::m_BoxDimensions) <= sizeof(dmVMath::Vector3), Invalid_Struct_Size);

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params.m_Context;

        if (params.m_MaxComponentInstances == 0 || physics_context->m_Context == 0x0)
        {
            *params.m_World = 0x0;
            return dmGameObject::CREATE_RESULT_OK;
        }

        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, physics_context->m_BaseContext.m_MaxCollisionObjectCount);
        dmPhysics::NewWorldParams world_params;
        world_params.m_GetWorldTransformCallback = GetWorldTransform;
        world_params.m_SetWorldTransformCallback = SetWorldTransform;
        world_params.m_MaxCollisionObjectsCount  = comp_count;

        dmPhysics::HWorld2D physics_world = dmPhysics::NewWorld2D(physics_context->m_Context, world_params);
        if (physics_world == 0x0)
        {
            *params.m_World = 0x0;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        InstallBox2DPhysicsAdapter();

        CollisionWorldBox2D* world = new CollisionWorldBox2D();
        memset(world, 0, sizeof(CollisionWorldBox2D));

        world->m_BaseWorld.m_AdapterFunctions = g_PhysicsAdapter;
        world->m_World2D                      = physics_world;
        world->m_ComponentTypeIndex           = params.m_ComponentIndex;
        world->m_FirstUpdate                  = 1;
        world->m_Components.SetCapacity(comp_count);

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBox2DDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params.m_Context;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*) params.m_World;

        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        dmPhysics::DeleteWorld2D(physics_context->m_Context, world->m_World2D);

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void GetWorldTransform(void* user_data, dmTransform::Transform& world_transform)
    {
        if (!user_data)
        {
            return;
        }

        CollisionComponent* component = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;
        world_transform                  = dmGameObject::GetWorldTransform(instance);
    }

    static void SetWorldTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation)
    {
        if (!user_data)
        {
            return;
        }

        CollisionComponent* component    = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;

        // Preserve z for 2D physics
        dmVMath::Point3 p = dmGameObject::GetPosition(instance);
        p.setX(position.getX());
        p.setY(position.getY());
        dmGameObject::SetPosition(instance, p);
        dmGameObject::SetRotation(instance, rotation);

        ++g_NumPhysicsTransformsUpdated;
    }

    static void SetupEmptyTileGrid(CollisionWorldBox2D* world, CollisionComponentBox2D* component)
    {
        CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*) component->m_BaseComponent.m_Resource;
        if (resource->m_TileGrid)
        {
            dmPhysics::ClearGridShapeHulls(component->m_Object2D);
        }
    }

    static void SetCollisionObjectData(CollisionWorldBox2D* world, CollisionComponentBox2D* component, CollisionObjectResourceBox2D* resource, dmPhysicsDDF::CollisionObjectDesc* ddf, bool enabled, dmPhysics::CollisionObjectData &out_data)
    {
        CollisionObjectResource* resource_base = (CollisionObjectResource*) resource;

        out_data.m_UserData = component;
        out_data.m_Type = (dmPhysics::CollisionObjectType)ddf->m_Type;
        out_data.m_Mass = ddf->m_Mass;
        out_data.m_Friction = ddf->m_Friction;
        out_data.m_Restitution = ddf->m_Restitution;
        out_data.m_Group = GetGroupBitIndex(&world->m_BaseWorld, resource_base->m_Group, false);
        out_data.m_Mask = 0;
        out_data.m_LinearDamping = ddf->m_LinearDamping;
        out_data.m_AngularDamping = ddf->m_AngularDamping;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_Bullet = ddf->m_Bullet;
        out_data.m_Enabled = enabled;
        for (uint32_t i = 0; i < 16 && resource_base->m_Mask[i] != 0; ++i)
        {
            out_data.m_Mask |= GetGroupBitIndex(&world->m_BaseWorld, resource_base->m_Mask[i], false);
        }
    }

    static void SetupTileGrid(CollisionWorldBox2D* world, CollisionComponentBox2D* component) {

        CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*) component->m_BaseComponent.m_Resource;

        if (resource->m_TileGrid)
        {
            TileGridResource* tile_grid_resource = resource->m_TileGridResource;
            dmGameSystemDDF::TileGrid* tile_grid = tile_grid_resource->m_TileGrid;
            dmArray<dmPhysics::HCollisionShape2D>& shapes = resource->m_TileGridResource->m_GridShapes;
            uint32_t shape_count = shapes.Size();
            dmPhysics::HullFlags flags;

            TextureSetResource* texture_set_resource = tile_grid_resource->m_TextureSet;
            dmGameSystemDDF::TextureSet* tile_set = texture_set_resource->m_TextureSet;

            for (uint32_t i = 0; i < shape_count; ++i)
            {
                dmGameSystemDDF::TileLayer* layer = &tile_grid->m_Layers[i];

                // Set non-empty tiles
                uint32_t cell_count = layer->m_Cell.m_Count;
                for (uint32_t j = 0; j < cell_count; ++j)
                {
                    dmGameSystemDDF::TileCell* cell = &layer->m_Cell[j];
                    uint32_t tile = cell->m_Tile;

                    if (tile < tile_set->m_ConvexHulls.m_Count && tile_set->m_ConvexHulls[tile].m_Count > 0)
                    {
                        uint32_t cell_x = cell->m_X - tile_grid_resource->m_MinCellX;
                        uint32_t cell_y = cell->m_Y - tile_grid_resource->m_MinCellY;
                        flags.m_FlipHorizontal = cell->m_HFlip;
                        flags.m_FlipVertical = cell->m_VFlip;
                        flags.m_Rotate90 = cell->m_Rotate90;
                        dmPhysics::SetGridShapeHull(component->m_Object2D, i, cell_y, cell_x, tile, flags);
                        uint32_t child = cell_x + tile_grid_resource->m_ColumnCount * cell_y;
                        uint16_t group = GetGroupBitIndex(&world->m_BaseWorld, texture_set_resource->m_HullCollisionGroups[tile], false);

                        if (group != 0)
                        {
                            dmPhysics::CreateGridCellShape(component->m_Object2D, i, child);
                        }

                        dmPhysics::SetCollisionObjectFilter(component->m_Object2D, i, child, group, component->m_BaseComponent.m_Mask);
                    }
                }

                dmPhysics::SetGridShapeEnable(component->m_Object2D, i, layer->m_IsVisible);
            }
        }
    }

    static bool CreateCollisionObject(PhysicsContextBox2D* physics_context, CollisionWorldBox2D* world, dmGameObject::HInstance instance, CollisionComponentBox2D* component, bool enabled)
    {
        if (world == 0x0)
        {
            return false;
        }

        CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*) component->m_BaseComponent.m_Resource;
        CollisionObjectResource* resource_base = (CollisionObjectResource*) resource;

        dmPhysicsDDF::CollisionObjectDesc* ddf = resource_base->m_DDF;
        dmPhysics::CollisionObjectData data;
        SetCollisionObjectData(world, component, resource, ddf, enabled, data);
        component->m_BaseComponent.m_Mask = data.m_Mask;

        component->m_BaseComponent.m_EventMask  = ddf->m_EventCollision?EVENT_MASK_COLLISION:0;
        component->m_BaseComponent.m_EventMask |= ddf->m_EventContact?EVENT_MASK_CONTACT:0;
        component->m_BaseComponent.m_EventMask |= ddf->m_EventTrigger?EVENT_MASK_TRIGGER:0;

        dmPhysics::HWorld2D physics_world = world->m_World2D;
        dmPhysics::HCollisionObject2D collision_object = 0;
        collision_object = dmPhysics::NewCollisionObject2D(physics_world, data,
                                                           resource->m_Shapes2D,
                                                           resource_base->m_ShapeTranslation,
                                                           resource_base->m_ShapeRotation,
                                                           resource_base->m_ShapeCount);

        if (collision_object == 0x0)
        {
            return false;
        }

        if (component->m_Object2D != 0x0)
        {
            dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
        }
        component->m_Object2D = collision_object;

        SetupEmptyTileGrid(world,component);
        SetupTileGrid(world, component);
        return true;
    }

    dmGameObject::CreateResult CompCollisionObjectBox2DCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollisionObjectResource* co_res = (CollisionObjectResource*)params.m_Resource;
        if (co_res == 0x0 || co_res->m_DDF == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if ((co_res->m_DDF->m_Mass == 0.0f && co_res->m_DDF->m_Type == dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC) ||
            (co_res->m_DDF->m_Mass > 0.0f && co_res->m_DDF->m_Type != dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC))
        {
            dmLogError("Invalid mass %f for shape type %d", co_res->m_DDF->m_Mass, co_res->m_DDF->m_Type);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params.m_Context;

        CollisionComponentBox2D* component = new CollisionComponentBox2D();
        component->m_Joints          = 0x0;
        component->m_JointEndPoints  = 0x0;
        component->m_Object2D        = 0;
        component->m_FlippedX        = 0;
        component->m_FlippedY        = 0;

        CollisionComponent* component_base = &component->m_BaseComponent;
        component_base->m_Resource         = (CollisionObjectResource*) params.m_Resource;
        component_base->m_Instance         = params.m_Instance;
        component_base->m_ComponentIndex   = params.m_ComponentIndex;
        component_base->m_AddedToUpdate    = false;
        component_base->m_StartAsEnabled   = true;

        CollisionWorldBox2D* world = (CollisionWorldBox2D*) params.m_World;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, false))
        {
            delete component;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBox2DDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)*params.m_UserData;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;

        // Destroy joint ends
        JointEndPoint* joint_end = component->m_JointEndPoints;
        while (joint_end) {
            JointEntry* joint_entry = joint_end->m_JointEntry;
            DeleteJoint(world, joint_entry->m_Joint);
            joint_entry->m_Joint = 0x0;

            JointEndPoint* next = joint_end->m_Next;
            delete joint_end;
            joint_end = next;
        }
        component->m_JointEndPoints = 0x0;

        // Destroy joints
        JointEntry* joint_entry = component->m_Joints;
        while (joint_entry)
        {
            if (joint_entry->m_Joint)
            {
                DeleteJoint(world, joint_entry);
            }
            JointEntry* next = joint_entry->m_Next;
            delete joint_entry;
            joint_entry = next;
        }
        component->m_Joints = 0x0;

        if (component->m_Object2D != 0)
        {
            dmPhysics::HWorld2D physics_world = world->m_World2D;
            dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
            component->m_Object2D = 0;
        }

        uint32_t num_components = world->m_Components.Size();
        for (uint32_t i = 0; i < num_components; ++i)
        {
            CollisionComponentBox2D* c = world->m_Components[i];
            if (c == component)
            {
                world->m_Components.EraseSwap(i);
                break;
            }
        }

        delete component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBox2DFinal(const dmGameObject::ComponentFinalParams& params)
    {
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        component->m_AddedToUpdate = false;
        component->m_StartAsEnabled = true;
        return dmGameObject::CREATE_RESULT_OK;
    }
    dmGameObject::CreateResult CompCollisionObjectBox2DAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;
        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if (world->m_Components.Full())
        {
            ShowFullBufferError("Collision object", PHYSICS_MAX_COLLISION_OBJECTS_KEY, world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)*params.m_UserData;
        assert(!component->m_BaseComponent.m_AddedToUpdate);

        dmPhysics::SetEnabled2D(world->m_World2D, component->m_Object2D, component->m_BaseComponent.m_StartAsEnabled);
        component->m_BaseComponent.m_AddedToUpdate = true;

        world->m_Components.Push(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DispatchCallback(dmMessage::Message* message, void* user_ptr)
    {
        DispatchContext* context = (DispatchContext*)user_ptr;

        if (message->m_Descriptor == 0)
            return;

        dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
        if (descriptor == dmPhysicsDDF::RequestRayCast::m_DDFDescriptor)
        {
            dmPhysicsDDF::RequestRayCast* ddf = (dmPhysicsDDF::RequestRayCast*)message->m_Data;

            dmhash_t coll_name_hash = dmMessage::GetSocketNameHash(message->m_Sender.m_Socket);

            // Target collection which can be different than we are updating for.
            dmGameObject::HCollection collection = dmGameObject::GetCollectionByHash(context->m_Register, coll_name_hash);
            if (!collection) // if the collection has been removed
                return;

            // NOTE! The collision world for the target collection is looked up using this worlds component index
            //       which is assumed to be the same as in the target collection.
            uint32_t component_type_index = context->m_ComponentTypeIndex;
            CollisionWorldBox2D* world = (CollisionWorldBox2D*) dmGameObject::GetWorld(collection, component_type_index);

            // Give that the assumption above holds, this assert will hold too.
            assert(world->m_ComponentTypeIndex == component_type_index);

            dmPhysics::RayCastRequest request;
            request.m_From = ddf->m_From;
            request.m_To = ddf->m_To;
            request.m_Mask = ddf->m_Mask;
            request.m_UserId = (ddf->m_RequestId & 0xff);
            dmVMath::Vector3 from2d = dmVMath::Vector3(ddf->m_From);
            dmVMath::Vector3 to2d = dmVMath::Vector3(ddf->m_To);
            from2d.setZ(0.0f);
            to2d.setZ(0.0f);
            if (dmVMath::LengthSqr(to2d - from2d) <= 0.0f)
            {
                return;
            }

            dmMessage::URL* receiver = (dmMessage::URL*)malloc(sizeof(dmMessage::URL));
            memcpy(receiver, &message->m_Sender, sizeof(*receiver));

            request.m_UserData = (void*)receiver;
            request.m_IgnoredUserData = 0;

            if (!dmPhysics::RequestRayCast2D(world->m_World2D, request))
            {
                free(receiver);
                return;
            }
        }
    }

    // This function will dispatch on the (global) physics socket, and will potentially handle messages belonging to other collections
    // than the current one being updated.
    //
    // TODO: Make a nicer solution for this, perhaps a per-collection physics socket
    static bool CompCollisionObjectDispatchPhysicsMessages(PhysicsContextBox2D *physics_context, CollisionWorldBox2D *world, dmGameObject::HCollection collection)
    {
        DispatchContext dispatch_context;
        dispatch_context.m_PhysicsContext = physics_context;
        dispatch_context.m_Success = true;
        dispatch_context.m_ComponentTypeIndex = world->m_ComponentTypeIndex;
        dispatch_context.m_Register = dmGameObject::GetRegister(collection);

        dmMessage::HSocket physics_socket;
        physics_socket = dmPhysics::GetSocket2D(physics_context->m_Context);

        dmMessage::Dispatch(physics_socket, DispatchCallback, (void*)&dispatch_context);
        return dispatch_context.m_Success;
    }

    static void Step(CollisionWorldBox2D* world, PhysicsContextBox2D* physics_context, dmGameObject::HCollection collection, const dmPhysics::StepWorldContext* step_ctx)
    {
        CollisionUserData* collision_user_data = (CollisionUserData*)step_ctx->m_CollisionUserData;
        CollisionUserData* contact_user_data = (CollisionUserData*)step_ctx->m_ContactPointUserData;

        g_NumPhysicsTransformsUpdated = 0;

        world->m_CurrentDT = step_ctx->m_DT;

        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, collection))
        {
            dmLogWarning("Failed to dispatch physics messages");
        }

        DM_PROFILE("StepWorld2D");
        dmPhysics::StepWorld2D(world->m_World2D, *step_ctx);

        if (collision_user_data->m_Count >= physics_context->m_BaseContext.m_MaxCollisionCount && physics_context->m_BaseContext.m_MaxCollisionCount > 0)
        {
            if (!g_CollisionOverflowWarning)
            {
                dmLogWarning("Maximum number of collisions (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_BaseContext.m_MaxCollisionCount, PHYSICS_MAX_COLLISIONS_KEY);
                g_CollisionOverflowWarning = true;
            }
        }
        else
        {
            g_CollisionOverflowWarning = false;
        }
        if (contact_user_data->m_Count >= physics_context->m_BaseContext.m_MaxContactPointCount && physics_context->m_BaseContext.m_MaxContactPointCount > 0)
        {
            if (!g_ContactOverflowWarning)
            {
                dmLogWarning("Maximum number of contacts (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_BaseContext.m_MaxContactPointCount, PHYSICS_MAX_CONTACTS_KEY);
                g_ContactOverflowWarning = true;
            }
        }
        else
        {
            g_ContactOverflowWarning = false;
        }

        dmMessage::HSocket socket = dmGameObject::GetMessageSocket(collection);
        dmGameObject::DispatchMessages(collection, &socket, 1);

        if (g_NumPhysicsTransformsUpdated > 0)
        {
            dmGameObject::UpdateTransforms(collection);
        }
    }

    static dmGameObject::UpdateResult CompCollisionObjectUpdateInternal(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;

        // Hot-reload is not available in release, so lets not iterate collision components in that case.
        if (dLib::IsDebugMode())
        {
            uint32_t num_components = world->m_Components.Size();
            for (uint32_t i = 0; i < num_components; ++i)
            {
                CollisionComponentBox2D* c = world->m_Components[i];
                CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*)c->m_BaseComponent.m_Resource;

                TileGridResource* tile_grid_res = resource->m_TileGridResource;
                if (tile_grid_res != 0x0 && tile_grid_res->m_Dirty)
                {
                    dmPhysicsDDF::CollisionObjectDesc* ddf = resource->m_BaseResource.m_DDF;
                    dmPhysics::CollisionObjectData data;
                    SetCollisionObjectData(world, c, resource, ddf, true, data);
                    c->m_BaseComponent.m_Mask = data.m_Mask;

                    dmPhysics::DeleteCollisionObject2D(world->m_World2D, c->m_Object2D);
                    dmArray<dmPhysics::HCollisionShape2D>& shapes = resource->m_TileGridResource->m_GridShapes;
                    c->m_Object2D = dmPhysics::NewCollisionObject2D(world->m_World2D, data, &shapes.Front(), shapes.Size());

                    SetupEmptyTileGrid(world, c);
                    SetupTileGrid(world, c);
                    tile_grid_res->m_Dirty = 0;
                }
            }
        }

        CollisionUserData collision_user_data;
        collision_user_data.m_World = &world->m_BaseWorld;
        collision_user_data.m_Context = &physics_context->m_BaseContext;
        collision_user_data.m_Count = 0;
        CollisionUserData contact_user_data;
        contact_user_data.m_World = &world->m_BaseWorld;
        contact_user_data.m_Context = &physics_context->m_BaseContext;
        contact_user_data.m_Count = 0;

        dmPhysics::StepWorldContext step_world_context;
        step_world_context.m_CollisionCallback = CollisionCallback;
        step_world_context.m_CollisionUserData = &collision_user_data;
        step_world_context.m_ContactPointCallback = ContactPointCallback;
        step_world_context.m_ContactPointUserData = &contact_user_data;
        step_world_context.m_TriggerEnteredCallback = TriggerEnteredCallback;
        step_world_context.m_TriggerEnteredUserData = world;
        step_world_context.m_TriggerExitedCallback = TriggerExitedCallback;
        step_world_context.m_TriggerExitedUserData = world;
        step_world_context.m_RayCastCallback = RayCastCallback;
        step_world_context.m_RayCastUserData = world;
        step_world_context.m_FixedTimeStep = physics_context->m_BaseContext.m_UseFixedTimestep;
        step_world_context.m_MaxFixedTimeSteps = physics_context->m_BaseContext.m_MaxFixedTimesteps;
        step_world_context.m_Box2DVelocityIterations = physics_context->m_BaseContext.m_Box2DVelocityIterations;
        step_world_context.m_Box2DPositionIterations = physics_context->m_BaseContext.m_Box2DPositionIterations;
        step_world_context.m_Box2DSubStepCount = physics_context->m_BaseContext.m_Box2DSubStepCount;

        step_world_context.m_DT = params.m_UpdateContext->m_DT;

        Step(world, physics_context, params.m_Collection, &step_world_context);

        // We only want to call this once per frame
        dmPhysics::SetDrawDebug2D(world->m_World2D, physics_context->m_BaseContext.m_Debug);

        DM_PROPERTY_ADD_U32(rmtp_CollisionObjectBox2D, world->m_Components.Size());
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectBox2DUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        if (physics_context->m_BaseContext.m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the fixed update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }
    dmGameObject::UpdateResult CompCollisionObjectBox2DFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        if (!physics_context->m_BaseContext.m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the dynamic update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }
    dmGameObject::UpdateResult CompCollisionObjectBox2DPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;

        // Dispatch also in post-messages since messages might have been posting from script components, or init
        // functions in factories, and they should not linger around to next frame (which might not come around)
        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, params.m_Collection))
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;

        return dmGameObject::UPDATE_RESULT_OK;
    }
    dmGameObject::UpdateResult CompCollisionObjectBox2DOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*) *params.m_UserData;
        CollisionComponent* component_base = (CollisionComponent*) *params.m_UserData;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;

        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash ||
            params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            bool enable = false;
            if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
            {
                enable = true;
            }

            if (component_base->m_AddedToUpdate)
            {
                dmPhysics::SetEnabled2D(world->m_World2D, component->m_Object2D, enable);
            }
            else
            {
                // Deferred controlling the enabled state. Objects are force disabled until
                // they are added to update.
                component_base->m_StartAsEnabled = enable;
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::ApplyForce::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::ApplyForce* af = (dmPhysicsDDF::ApplyForce*) params.m_Message->m_Data;
            dmPhysics::ApplyForce2D(physics_context->m_Context, component->m_Object2D, af->m_Force, af->m_Position);
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::RequestVelocity::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::VelocityResponse response;
            response.m_LinearVelocity = dmPhysics::GetLinearVelocity2D(physics_context->m_Context, component->m_Object2D);
            response.m_AngularVelocity = dmPhysics::GetAngularVelocity2D(physics_context->m_Context, component->m_Object2D);
            dmhash_t message_id = dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_NameHash;
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::VelocityResponse::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::VelocityResponse);
            dmMessage::Result result = dmMessage::Post(&params.m_Message->m_Receiver, &params.m_Message->m_Sender, message_id, 0, descriptor, &response, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send %s to component, result: %d.", dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_Name, result);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_NameHash)
        {
            CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*) component->m_BaseComponent.m_Resource;
            if (resource->m_TileGrid == 0)
            {
                dmLogError("Hulls can only be set for collision objects with tile grids as shape.");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            dmPhysicsDDF::SetGridShapeHull* ddf = (dmPhysicsDDF::SetGridShapeHull*) params.m_Message->m_Data;
            uint32_t column = ddf->m_Column;
            uint32_t row = ddf->m_Row;
            uint32_t hull = ddf->m_Hull;

            TileGridResource* tile_grid_resource = resource->m_TileGridResource;

            if (row >= tile_grid_resource->m_RowCount || column >= tile_grid_resource->m_ColumnCount)
            {
                dmLogError("SetGridShapeHull: <row,column> out of bounds");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            if (hull != ~0u && hull >= tile_grid_resource->m_TextureSet->m_HullCollisionGroups.Size())
            {
                dmLogError("SetGridShapHull: specified hull index is out of bounds.");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }

            dmPhysics::HullFlags flags;
            flags.m_FlipHorizontal = ddf->m_FlipHorizontal;
            flags.m_FlipVertical = ddf->m_FlipVertical;
            flags.m_Rotate90 = ddf->m_Rotate90;
            bool success = dmPhysics::SetGridShapeHull(component->m_Object2D, ddf->m_Shape, row, column, hull, flags);
            if (!success)
            {
                dmLogError("SetGridShapeHull: unable to set hull %d for shape %d", hull, ddf->m_Shape);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            uint32_t child = column + tile_grid_resource->m_ColumnCount * row;
            uint16_t group = 0;
            uint16_t mask = 0;
            // Hull-index of 0xffffffff is empty cell
            if (hull != ~0u)
            {
                group = GetGroupBitIndex((CollisionWorld*)params.m_World, tile_grid_resource->m_TextureSet->m_HullCollisionGroups[hull], false);
                mask = component->m_BaseComponent.m_Mask;
            }
            dmPhysics::SetCollisionObjectFilter(component->m_Object2D, ddf->m_Shape, child, group, mask);
        }
        else if(params.m_Message->m_Id == dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor->m_NameHash)
        {
            CollisionObjectResourceBox2D* resource = (CollisionObjectResourceBox2D*) component->m_BaseComponent.m_Resource;
            if (resource->m_TileGrid == 0)
            {
                dmLogError("Layer visibility can only be set on tile grids");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            dmPhysicsDDF::EnableGridShapeLayer* ddf = (dmPhysicsDDF::EnableGridShapeLayer*) params.m_Message->m_Data;
            dmPhysics::SetGridShapeEnable(component->m_Object2D, ddf->m_Shape, ddf->m_Enable);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCollisionObjectBox2DOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)params.m_World;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)*params.m_UserData;
        component->m_BaseComponent.m_Resource = (CollisionObjectResource*)params.m_Resource;
        component->m_BaseComponent.m_AddedToUpdate = false;
        component->m_BaseComponent.m_StartAsEnabled = true;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, true))
        {
            dmLogError("%s", "Could not recreate collision object component, not reloaded.");
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectBox2DGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)*params.m_UserData;
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;
        if (params.m_PropertyId == PROP_LINEAR_VELOCITY) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearVelocity2D(physics_context->m_Context, component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularVelocity2D(physics_context->m_Context, component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_MASS) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetMass2D(component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_BULLET) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::IsBullet2D(component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_LINEAR_DAMPING) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearDamping2D(component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_ANGULAR_DAMPING) {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularDamping2D(component->m_Object2D));
            return dmGameObject::PROPERTY_RESULT_OK;
        } else {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectBox2DSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)*params.m_UserData;
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*)params.m_Context;

        if (params.m_PropertyId == PROP_LINEAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetLinearVelocity2D(physics_context->m_Context, component->m_Object2D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetAngularVelocity2D(physics_context->m_Context, component->m_Object2D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_BULLET)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetBullet2D(component->m_Object2D, params.m_Value.m_Bool);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_LINEAR_DAMPING) {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetLinearDamping2D(component->m_Object2D, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_DAMPING)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetAngularDamping2D(component->m_Object2D, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MASS)
        {
            return dmGameObject::PROPERTY_RESULT_READ_ONLY;
        }
        else
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }
    }

    // Internal script API
    void* CompCollisionObjectGetBox2DWorld(dmGameObject::HComponentWorld _world)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        return dmPhysics::GetWorldContext2D(world->m_World2D);
    }

    void* CompCollisionObjectGetBox2DBody(dmGameObject::HComponent _component)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        return dmPhysics::GetCollisionObjectContext2D(component->m_Object2D);
    }

    // Adapter functions
    static bool IsEnabledBox2D(CollisionWorld* world, CollisionComponent* component)
    {
        CollisionComponentBox2D* _component = (CollisionComponentBox2D*) component;
        return dmPhysics::IsEnabled2D(_component->m_Object2D);
    }

    static void WakeupCollisionBox2D(CollisionWorld* _world, CollisionComponent* _component)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*) _component;
        dmPhysics::Wakeup2D(component->m_Object2D);
    }

    static void RayCastBox2D(CollisionWorld* _world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        dmPhysics::RayCast2D(world->m_World2D, request, results);
    }

    static void DeleteJoint(CollisionWorldBox2D* world, JointEntry* joint_entry)
    {
        assert(joint_entry);

        DeleteJoint(world, joint_entry->m_Joint);
        joint_entry->m_Joint = 0x0;

        // Remove end point
        assert(joint_entry->m_EndPoint);
        JointEndPoint* end_point = joint_entry->m_EndPoint;
        CollisionComponentBox2D* owner_component = end_point->m_Owner;

        // Find and remove end point from previous component_b
        bool removed = false;
        JointEndPoint* end_point_prev = 0x0;
        JointEndPoint* end_point_next = owner_component->m_JointEndPoints;
        while (end_point_next) {
            if (end_point_next == end_point)
            {
                if (end_point_prev) {
                    end_point_prev->m_Next = end_point_next->m_Next;
                } else {
                    owner_component->m_JointEndPoints = end_point_next->m_Next;
                }

                removed = true;
                break;
            }
            end_point_prev = end_point_next;
            end_point_next = end_point_next->m_Next;
        }

        assert(removed);
        delete end_point;
    }

    static void DeleteJoint(CollisionWorldBox2D* world, dmPhysics::HJoint joint)
    {
        assert(joint);
        dmPhysics::DeleteJoint2D(world->m_World2D, joint);
    }

    // Find a JointEntry in the linked list of a collision component based on the joint id.
    static JointEntry* FindJointEntry(CollisionWorldBox2D* world, CollisionComponentBox2D* component, dmhash_t id)
    {
        JointEntry* joint_entry = component->m_Joints;
        while (joint_entry)
        {
            if (joint_entry->m_Id == id)
            {
                return joint_entry;
            }
            joint_entry = joint_entry->m_Next;
        }

        return 0x0;
    }

    // Connects a joint between two components, a JointEntry with the id must exist for this to succeed.
    static dmPhysics::JointResult CreateJointBox2D(CollisionWorld* _world, CollisionComponent* _component_a, dmhash_t id, const dmVMath::Point3& apos, CollisionComponent* _component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*) _world;

        if (dmPhysics::IsWorldLocked(world->m_World2D))
        {
            return dmPhysics::RESULT_PHYSICS_WORLD_LOCKED;
        }

        CollisionComponentBox2D* component_a = (CollisionComponentBox2D*)_component_a;
        CollisionComponentBox2D* component_b = (CollisionComponentBox2D*)_component_b;

        // Check if there is already a joint with this id
        JointEntry* joint_entry = FindJointEntry(world, component_a, id);
        if (joint_entry)
        {
            return dmPhysics::RESULT_ID_EXISTS;
        }

        dmPhysics::HJoint joint_handle = dmPhysics::CreateJoint2D(world->m_World2D, component_a->m_Object2D, apos, component_b->m_Object2D, bpos, type, joint_params);

        // NOTE: For the future, we might think about preallocating these structs
        // in batches (when needed) and store them in an object pool.
        // - so that when deleting a collision world, it's easy to delete everything quickly (as opposed to traversing each component).
        // - so we can avoid the frequent new/delete.

        // Create new joint entry to hold the generic joint handle
        joint_entry = new JointEntry(id, joint_handle, component_a->m_Joints);
        component_a->m_Joints = joint_entry;
        joint_entry->m_Type = type;

        // Setup a joint end point for component B.
        JointEndPoint* new_end_point = new JointEndPoint();
        new_end_point->m_Next = component_b->m_JointEndPoints;
        new_end_point->m_JointEntry = joint_entry;
        new_end_point->m_Owner = component_b;
        component_b->m_JointEndPoints = new_end_point;

        joint_entry->m_EndPoint = new_end_point;

        return dmPhysics::RESULT_OK;
    }

    static dmPhysics::JointResult DestroyJointBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;

        JointEntry* joint_entry = FindJointEntry(world, component, id);
        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        DeleteJoint(world, joint_entry);

        // Remove joint entry from list on component instance
        if (component->m_Joints == joint_entry)
        {
            component->m_Joints = joint_entry->m_Next;
        }
        else
        {
            JointEntry* j = component->m_Joints;
            while (j)
            {
                if (j->m_Next == joint_entry)
                {
                    j->m_Next = joint_entry->m_Next;
                    break;
                }
                j = j->m_Next;
            }
        }

        delete joint_entry;

        return dmPhysics::RESULT_OK;
    }

    static dmPhysics::JointResult GetJointParamsBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        joint_type = joint_entry->m_Type;
        bool r = GetJointParams2D(world->m_World2D, joint_entry->m_Joint, joint_entry->m_Type, joint_params);
        return (r ? dmPhysics::RESULT_OK : dmPhysics::RESULT_UNKNOWN_ERROR);
    }

    static dmPhysics::JointResult GetJointTypeBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id, dmPhysics::JointType& joint_type)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry)
        {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        joint_type = joint_entry->m_Type;

        return dmPhysics::RESULT_OK;
    }

    static dmPhysics::JointResult SetJointParamsBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        bool r = SetJointParams2D(world->m_World2D, joint_entry->m_Joint, joint_entry->m_Type, joint_params);
        return (r ? dmPhysics::RESULT_OK : dmPhysics::RESULT_UNKNOWN_ERROR);
    }

    static dmPhysics::JointResult GetJointReactionForceBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id, dmVMath::Vector3& force)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        bool r = GetJointReactionForce2D(world->m_World2D, joint_entry->m_Joint, force, 1.0f / world->m_CurrentDT);
        return (r ? dmPhysics::RESULT_OK : dmPhysics::RESULT_UNKNOWN_ERROR);
    }

    static dmPhysics::JointResult GetJointReactionTorqueBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t id, float& torque)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        bool r = GetJointReactionTorque2D(world->m_World2D, joint_entry->m_Joint, torque, 1.0f / world->m_CurrentDT);
        return (r ? dmPhysics::RESULT_OK : dmPhysics::RESULT_UNKNOWN_ERROR);
    }

    static void SetGravityBox2D(CollisionWorld* _world, const dmVMath::Vector3& gravity)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        dmPhysics::SetGravity2D(world->m_World2D, gravity);
    }

    static dmVMath::Vector3 GetGravityBox2D(CollisionWorld* _world)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        return dmPhysics::GetGravity2D(world->m_World2D);
    }

    static void SetCollisionFlipHBox2D(CollisionWorld* _world, CollisionComponent* _component, bool flip)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        if (component->m_FlippedX != flip)
        {
            CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
            dmPhysics::FlipH2D(world->m_World2D, component->m_Object2D);
        }
        component->m_FlippedX = flip;
    }

    static void SetCollisionFlipVBox2D(CollisionWorld* _world, CollisionComponent* _component, bool flip)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        if (component->m_FlippedY != flip)
        {
            CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
            dmPhysics::FlipV2D(world->m_World2D, component->m_Object2D);
        }
        component->m_FlippedY = flip;
    }

    static dmhash_t GetCollisionGroupBox2D(CollisionWorld* _world, CollisionComponent* _component)
    {
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        uint16_t groupbit = dmPhysics::GetGroup2D(world->m_World2D, component->m_Object2D);
        return GetLSBGroupHash(_world, groupbit);
    }

    // returns false if no such collision group has been registered
    static bool SetCollisionGroupBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit)
        {
            return false; // error. No such group.
        }

        dmPhysics::SetGroup2D(world->m_World2D, component->m_Object2D, groupbit);
        return true; // all good
    }

    // Updates 'maskbit' with the mask value. Returns false if no such collision group has been registered.
    static bool GetCollisionMaskBitBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash, bool* maskbit)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit) {
            return false;
        }

        *maskbit = dmPhysics::GetMaskBit2D(world->m_World2D, component->m_Object2D, groupbit);
        return true;
    }

    // returns false if no such collision group has been registered
    static bool SetCollisionMaskBitBox2D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash, bool boolvalue)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit)
        {
            return false;
        }

        dmPhysics::SetMaskBit2D(world->m_World2D, component->m_Object2D, groupbit, boolvalue);
        return true;
    }

    static void UpdateMassBox2D(CollisionWorld* _world, CollisionComponent* _component, float mass)
    {
        CollisionWorldBox2D* world = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*)_component;

        if(!dmPhysics::UpdateMass2D(world->m_World2D, component->m_Object2D, mass))
        {
            dmLogError("The Update Mass function can be used only for Dynamic objects with shape area > 0");
        }
    }

    static bool GetShapeBox2D(CollisionWorld* _world, CollisionComponent* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionWorldBox2D* world         = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*) _component;
        uint32_t shape_count               = _component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        shape_info->m_Type = _component->m_Resource->m_ShapeTypes[shape_ix];

        dmPhysics::HCollisionShape2D shape2d = dmPhysics::GetCollisionShape2D(world->m_World2D, component->m_Object2D, shape_ix);

        switch(shape_info->m_Type)
        {
            case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            {
                float sphere_radius;
                dmPhysics::GetCollisionShapeRadius2D(world->m_World2D, shape2d, &sphere_radius);
                shape_info->m_SphereDiameter = sphere_radius * 2.0f;
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            {
                shape_info->m_BoxDimensions[0] = 0.0f;
                shape_info->m_BoxDimensions[1] = 0.0f;
                shape_info->m_BoxDimensions[2] = 1.0f;
                dmPhysics::GetCollisionShapeBoxDimensions2D(world->m_World2D, shape2d, _component->m_Resource->m_ShapeRotation[shape_ix], shape_info->m_BoxDimensions[0], shape_info->m_BoxDimensions[1]);
            } break;
            default: assert(0);
        }
        return true;
    }

    static bool SetShapeBox2D(CollisionWorld* _world, CollisionComponent* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionWorldBox2D* world         = (CollisionWorldBox2D*)_world;
        CollisionComponentBox2D* component = (CollisionComponentBox2D*) _component;
        uint32_t shape_count               = _component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        dmPhysics::HCollisionShape2D shape2d = dmPhysics::GetCollisionShape2D(world->m_World2D, component->m_Object2D, shape_ix);

        switch(shape_info->m_Type)
        {
            case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            {
                dmPhysics::SetCollisionShapeRadius2D(world->m_World2D, shape2d, shape_info->m_SphereDiameter * 0.5f);
                dmPhysics::SynchronizeObject2D(world->m_World2D, component->m_Object2D);
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            {
                dmPhysics::SetCollisionShapeBoxDimensions2D(world->m_World2D, shape2d, _component->m_Resource->m_ShapeRotation[shape_ix], shape_info->m_BoxDimensions[0] * 0.5f, shape_info->m_BoxDimensions[1] * 0.5f);
            } break;
            default: assert(0);
        }

        return true;
    }

    static PhysicsEngineType GetPhysicsEngineTypeBox2D(CollisionWorld* _world)
    {
        return PhysicsEngineType::PHYSICS_ENGINE_BOX2D;
    }

    static void InstallBox2DPhysicsAdapter()
    {
        if (g_PhysicsAdapter)
        {
            return;
        }

        g_PhysicsAdapter = new PhysicsAdapterFunctionTable();
        g_PhysicsAdapter->m_IsEnabled              = IsEnabledBox2D;
        g_PhysicsAdapter->m_WakeupCollision        = WakeupCollisionBox2D;
        g_PhysicsAdapter->m_RayCast                = RayCastBox2D;
        g_PhysicsAdapter->m_SetGravity             = SetGravityBox2D;
        g_PhysicsAdapter->m_GetGravity             = GetGravityBox2D;
        g_PhysicsAdapter->m_SetCollisionFlipH      = SetCollisionFlipHBox2D;
        g_PhysicsAdapter->m_SetCollisionFlipV      = SetCollisionFlipVBox2D;
        g_PhysicsAdapter->m_GetCollisionGroup      = GetCollisionGroupBox2D;
        g_PhysicsAdapter->m_SetCollisionGroup      = SetCollisionGroupBox2D;
        g_PhysicsAdapter->m_GetCollisionMaskBit    = GetCollisionMaskBitBox2D;
        g_PhysicsAdapter->m_SetCollisionMaskBit    = SetCollisionMaskBitBox2D;
        g_PhysicsAdapter->m_UpdateMass             = UpdateMassBox2D;
        g_PhysicsAdapter->m_GetShapeIndex          = GetShapeIndexShared;
        g_PhysicsAdapter->m_GetShape               = GetShapeBox2D;
        g_PhysicsAdapter->m_SetShape               = SetShapeBox2D;
        g_PhysicsAdapter->m_CreateJoint            = CreateJointBox2D;
        g_PhysicsAdapter->m_DestroyJoint           = DestroyJointBox2D;
        g_PhysicsAdapter->m_GetJointParams         = GetJointParamsBox2D;
        g_PhysicsAdapter->m_GetJointType           = GetJointTypeBox2D;
        g_PhysicsAdapter->m_SetJointParams         = SetJointParamsBox2D;
        g_PhysicsAdapter->m_GetJointReactionForce  = GetJointReactionForceBox2D;
        g_PhysicsAdapter->m_GetJointReactionTorque = GetJointReactionTorqueBox2D;
        g_PhysicsAdapter->m_GetPhysicsEngineType   = GetPhysicsEngineTypeBox2D;
    }
}
