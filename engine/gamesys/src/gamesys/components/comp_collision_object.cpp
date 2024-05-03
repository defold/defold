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

#include "comp_collision_object.h"

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
#include "../resources/res_collision_object.h"
#include "../resources/res_textureset.h"
#include "../resources/res_tilegrid.h"

#include <gamesys/atlas_ddf.h>
#include <gamesys/texture_set_ddf.h>
#include <gamesys/gamesys_ddf.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_CollisionObject, 0, FrameReset, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

    /// Config key to use for tweaking maximum number of collision objects
    const char* PHYSICS_MAX_COLLISION_OBJECTS_KEY   = "physics.max_collision_object_count";
    /// Config key to use for tweaking maximum number of collisions reported
    const char* PHYSICS_MAX_COLLISIONS_KEY          = "physics.max_collisions";
    /// Config key to use for tweaking maximum number of contacts reported
    const char* PHYSICS_MAX_CONTACTS_KEY            = "physics.max_contacts";
    /// Config key for using fixed frame rate for the physics worlds
    const char* PHYSICS_USE_FIXED_TIMESTEP          = "physics.use_fixed_timestep";
    /// Config key for using max updates during a single step
    const char* PHYSICS_MAX_FIXED_TIMESTEPS         = "physics.max_fixed_timesteps";

    static const dmhash_t PROP_LINEAR_DAMPING = dmHashString64("linear_damping");
    static const dmhash_t PROP_ANGULAR_DAMPING = dmHashString64("angular_damping");
    static const dmhash_t PROP_LINEAR_VELOCITY = dmHashString64("linear_velocity");
    static const dmhash_t PROP_ANGULAR_VELOCITY = dmHashString64("angular_velocity");
    static const dmhash_t PROP_MASS = dmHashString64("mass");
    static const dmhash_t PROP_BULLET = dmHashString64("bullet");

    struct CollisionComponent;
    struct JointEndPoint;

    /// Joint entry that will keep track of joint connections from collision components.
    struct JointEntry
    {
        dmhash_t m_Id;
        dmPhysics::JointType m_Type;
        dmPhysics::HJoint m_Joint;
        JointEntry* m_Next;
        JointEndPoint* m_EndPoint;
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
        JointEndPoint* m_Next;
        CollisionComponent* m_Owner;
        JointEntry* m_JointEntry;
    };

    struct CollisionComponent
    {
        CollisionObjectResource* m_Resource;
        dmGameObject::HInstance m_Instance;

        union
        {
            dmPhysics::HCollisionObject3D m_Object3D;
            dmPhysics::HCollisionObject2D m_Object2D;
        };

        /// Linked list of joints FROM this component.
        JointEntry* m_Joints;

        /// Linked list of joints TO this component.
        JointEndPoint* m_JointEndPoints;

        dmPhysics::HCollisionShape3D* m_ShapeBuffer;

        uint16_t m_Mask;
        uint16_t m_ComponentIndex;
        // True if the physics is 3D
        // This is used to determine physics engine kind and to preserve
        // z for the 2d-case
        // A bit awkward to have a flag for this but we don't have access to
        // to PhysicsContext in the SetWorldTransform callback. This
        // could perhaps be improved.
        uint8_t m_3D : 1;

        // Tracking initial state.
        uint8_t m_AddedToUpdate : 1;
        uint8_t m_StartAsEnabled : 1;
        uint8_t m_FlippedX : 1; // set if it's been flipped
        uint8_t m_FlippedY : 1;
    };

    struct CollisionWorld
    {
        uint64_t m_Groups[16];

        void* m_CallbackInfo;

        union
        {
            dmPhysics::HWorld2D m_World2D;
            dmPhysics::HWorld3D m_World3D;
        };
        float       m_CurrentDT;    // Used to calculate joint reaction force and torque.
        float       m_AccumTime;    // Time saved from last component type update
        uint8_t     m_ComponentTypeIndex;
        uint8_t     m_3D : 1;
        uint8_t     m_FirstUpdate : 1;
        dmArray<CollisionComponent*> m_Components;
    };

    // Forward declarations
    static void DeleteJoint(CollisionWorld* world, dmPhysics::HJoint joint);
    static void DeleteJoint(CollisionWorld* world, JointEntry* joint_entry);

    static void GetWorldTransform(void* user_data, dmTransform::Transform& world_transform)
    {
        if (!user_data)
            return;
        CollisionComponent* component = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;
        world_transform = dmGameObject::GetWorldTransform(instance);
    }

    // TODO: Allow the SetWorldTransform to have a physics context which we can check instead!!
    static int g_NumPhysicsTransformsUpdated = 0;

    static void SetWorldTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation)
    {
        if (!user_data)
            return;
        CollisionComponent* component = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;
        if (component->m_3D)
        {
            dmGameObject::SetPosition(instance, position);
        }
        else
        {
            // Preserve z for 2D physics
            dmVMath::Point3 p = dmGameObject::GetPosition(instance);
            p.setX(position.getX());
            p.setY(position.getY());
            dmGameObject::SetPosition(instance, p);
        }
        dmGameObject::SetRotation(instance, rotation);
        ++g_NumPhysicsTransformsUpdated;
    }

    dmGameObject::CreateResult CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        DM_STATIC_ASSERT(sizeof(ShapeInfo::m_BoxDimensions) <= sizeof(dmVMath::Vector3), Invalid_Struct_Size);

        if (params.m_MaxComponentInstances == 0)
        {
            *params.m_World = 0x0;
            return dmGameObject::CREATE_RESULT_OK;
        }
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, physics_context->m_MaxCollisionObjectCount);
        dmPhysics::NewWorldParams world_params;
        world_params.m_GetWorldTransformCallback = GetWorldTransform;
        world_params.m_SetWorldTransformCallback = SetWorldTransform;
        world_params.m_MaxCollisionObjectsCount = comp_count;

        dmPhysics::HWorld2D world2D;
        dmPhysics::HWorld3D world3D;
        if (physics_context->m_3D)
        {
            world3D = dmPhysics::NewWorld3D(physics_context->m_Context3D, world_params);
            if (world3D == 0x0)
            {
                *params.m_World = 0x0;
                return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
            }
        }
        else
        {
            world2D = dmPhysics::NewWorld2D(physics_context->m_Context2D, world_params);
            if (world2D == 0x0)
            {
                *params.m_World = 0x0;
                return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
            }
        }

        CollisionWorld* world = new CollisionWorld();
        memset(world, 0, sizeof(CollisionWorld));
        if (physics_context->m_3D)
        {
            world->m_World3D = world3D;
        }
        else
        {
            world->m_World2D = world2D;
        }
        world->m_ComponentTypeIndex = params.m_ComponentIndex;
        world->m_3D = physics_context->m_3D;
        world->m_FirstUpdate = 1;
        world->m_Components.SetCapacity(comp_count);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionWorld* world = (CollisionWorld*)params.m_World;
        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if (physics_context->m_3D)
            dmPhysics::DeleteWorld3D(physics_context->m_Context3D, world->m_World3D);
        else
            dmPhysics::DeleteWorld2D(physics_context->m_Context2D, world->m_World2D);
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    /*
     * Looks into world->m_Groups index for the speficied group_hash. It returns its position as
     * bit index (a uint16_t with n-th bit set). If the hash is not found and we're in readonly mode
     * it will return 0. If readonly is false though, it assigns the hash to the first empty bit slot.
     * If there are no positions are left, it returns 0.
     */
    static uint16_t GetGroupBitIndex(CollisionWorld* world, uint64_t group_hash, bool readonly)
    {
        if (group_hash != 0)
        {
            for (uint32_t i = 0; i < 16; ++i)
            {
                if (world->m_Groups[i] != 0)
                {
                    if (world->m_Groups[i] == group_hash)
                    {
                        return 1 << i;
                    }
                }
                else if (readonly)
                {
                    return 0;
                } else
                {
                    world->m_Groups[i] = group_hash;
                    return 1 << i;
                }
            }

            // When we get here, there are no more group bits available
            dmLogWarning("The collision group '%s' could not be used since the maximum group count has been reached (16).", dmHashReverseSafe64(group_hash));
        }
        return 0;
    }

    // Converts a collision mask bitfield to the respective group hash. Takes into account only the least significant bit.
    uint64_t GetLSBGroupHash(void* _world, uint16_t mask)
    {
        if (mask > 0)
        {
            uint32_t index = 0;
            while ((mask & 1) == 0)
            {
                mask >>= 1;
                ++index;
            }
            CollisionWorld* world = (CollisionWorld*)_world;
            return world->m_Groups[index];
        }
        return 0;
    }

    static void SetupEmptyTileGrid(CollisionWorld* world, CollisionComponent* component)
    {
        if (component->m_Resource->m_TileGrid)
        {
            dmPhysics::ClearGridShapeHulls(component->m_Object2D);
        }
    }

    static void SetupTileGrid(CollisionWorld* world, CollisionComponent* component) {
        CollisionObjectResource* resource = component->m_Resource;
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
                        dmPhysics::SetGridShapeHull(component->m_Object2D, i, cell_y, cell_x, tile, flags);
                        uint32_t child = cell_x + tile_grid_resource->m_ColumnCount * cell_y;
                        uint16_t group = GetGroupBitIndex(world, texture_set_resource->m_HullCollisionGroups[tile], false);
                        dmPhysics::SetCollisionObjectFilter(component->m_Object2D, i, child, group, component->m_Mask);
                    }
                }

                dmPhysics::SetGridShapeEnable(component->m_Object2D, i, layer->m_IsVisible);
            }
        }
    }

    static void SetCollisionObjectData(CollisionWorld* world, CollisionComponent* component, CollisionObjectResource* resource, dmPhysicsDDF::CollisionObjectDesc* ddf, bool enabled, dmPhysics::CollisionObjectData &out_data)
    {
        out_data.m_UserData = component;
        out_data.m_Type = (dmPhysics::CollisionObjectType)ddf->m_Type;
        out_data.m_Mass = ddf->m_Mass;
        out_data.m_Friction = ddf->m_Friction;
        out_data.m_Restitution = ddf->m_Restitution;
        out_data.m_Group = GetGroupBitIndex(world, resource->m_Group, false);
        out_data.m_Mask = 0;
        out_data.m_LinearDamping = ddf->m_LinearDamping;
        out_data.m_AngularDamping = ddf->m_AngularDamping;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_Bullet = ddf->m_Bullet;
        out_data.m_Enabled = enabled;
        for (uint32_t i = 0; i < 16 && resource->m_Mask[i] != 0; ++i)
        {
            out_data.m_Mask |= GetGroupBitIndex(world, resource->m_Mask[i], false);
        }
    }

    static bool CreateCollisionObject(PhysicsContext* physics_context, CollisionWorld* world, dmGameObject::HInstance instance, CollisionComponent* component, bool enabled)
    {
        if (world == 0x0)
        {
            return false;
        }
        CollisionObjectResource* resource = component->m_Resource;
        dmPhysicsDDF::CollisionObjectDesc* ddf = resource->m_DDF;
        dmPhysics::CollisionObjectData data;
        SetCollisionObjectData(world, component, resource, ddf, enabled, data);
        component->m_Mask = data.m_Mask;
        if (physics_context->m_3D)
        {
            if (resource->m_TileGrid)
            {
                dmLogError("Collision objects in 3D can not have a tile grid as shape.");
                return false;
            }
            dmPhysics::HWorld3D physics_world = world->m_World3D;
            dmPhysics::HCollisionObject3D collision_object =
                    dmPhysics::NewCollisionObject3D(physics_world, data,
                                                    resource->m_Shapes3D,
                                                    resource->m_ShapeTranslation,
                                                    resource->m_ShapeRotation,
                                                    resource->m_ShapeCount);

            if (collision_object != 0x0)
            {
                if (component->m_Object3D != 0x0)
                    dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
                component->m_Object3D = collision_object;
            }
            else
            {
                return false;
            }
        }
        else
        {
            dmPhysics::HWorld2D physics_world = world->m_World2D;
            dmPhysics::HCollisionObject2D collision_object = 0;
            collision_object = dmPhysics::NewCollisionObject2D(physics_world, data,
                                                               resource->m_Shapes2D,
                                                               resource->m_ShapeTranslation,
                                                               resource->m_ShapeRotation,
                                                               resource->m_ShapeCount);

            if (collision_object != 0x0)
            {
                if (component->m_Object2D != 0x0)
                    dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
                component->m_Object2D = collision_object;

                SetupEmptyTileGrid(world,component);
                SetupTileGrid(world, component);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    dmGameObject::CreateResult CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollisionObjectResource* co_res = (CollisionObjectResource*)params.m_Resource;
        if (co_res == 0x0 || co_res->m_DDF == 0x0)
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        if ((co_res->m_DDF->m_Mass == 0.0f && co_res->m_DDF->m_Type == dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC)
            || (co_res->m_DDF->m_Mass > 0.0f && co_res->m_DDF->m_Type != dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC))
        {
            dmLogError("Invalid mass %f for shape type %d", co_res->m_DDF->m_Mass, co_res->m_DDF->m_Type);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionComponent* component = new CollisionComponent();
        component->m_3D = (uint8_t) physics_context->m_3D;
        component->m_Resource = (CollisionObjectResource*)params.m_Resource;
        component->m_Instance = params.m_Instance;
        component->m_Object2D = 0;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_AddedToUpdate = false;
        component->m_StartAsEnabled = true;
        component->m_Joints = 0x0;
        component->m_JointEndPoints = 0x0;
        component->m_FlippedX = 0;
        component->m_FlippedY = 0;
        component->m_ShapeBuffer = 0;

        CollisionWorld* world = (CollisionWorld*)params.m_World;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, false))
        {
            delete component;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params)
    {
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        component->m_AddedToUpdate = false;
        component->m_StartAsEnabled = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        CollisionWorld* world = (CollisionWorld*)params.m_World;

        delete[] component->m_ShapeBuffer;

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

        if (physics_context->m_3D)
        {
            if (component->m_Object3D != 0)
            {
                dmPhysics::HWorld3D physics_world = world->m_World3D;
                dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
                component->m_Object3D = 0;
            }
        }
        else
        {
            if (component->m_Object2D != 0)
            {
                dmPhysics::HWorld2D physics_world = world->m_World2D;
                dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
                component->m_Object2D = 0;
            }
        }

        uint32_t num_components = world->m_Components.Size();
        for (uint32_t i = 0; i < num_components; ++i)
        {
            CollisionComponent* c = world->m_Components[i];
            if (c == component)
            {
                world->m_Components.EraseSwap(i);
                break;
            }
        }

        delete component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    struct CollisionUserData
    {
        CollisionWorld* m_World;
        PhysicsContext* m_Context;
        uint32_t m_Count;
    };

    template <class DDFMessage>
    static void BroadCast(DDFMessage* ddf, dmGameObject::HInstance instance, dmhash_t instance_id, uint16_t component_index)
    {
        dmhash_t message_id = DDFMessage::m_DDFDescriptor->m_NameHash;
        uintptr_t descriptor = (uintptr_t)DDFMessage::m_DDFDescriptor;
        uint32_t data_size = sizeof(DDFMessage);
        dmMessage::URL sender;
        dmMessage::ResetURL(&sender);
        dmMessage::URL receiver;
        dmMessage::ResetURL(&receiver);
        receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance));
        receiver.m_Path = instance_id;
        // sender is the same as receiver, but with the specific collision object as fragment
        sender = receiver;
        dmGameObject::Result r = dmGameObject::GetComponentId(instance, component_index, &sender.m_Fragment);
        if (r != dmGameObject::RESULT_OK)
        {
            dmLogError("Could not retrieve sender component when reporting %s: %d", DDFMessage::m_DDFDescriptor->m_Name, r);
        }
        dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, ddf, data_size, 0);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not send %s to component: %d", DDFMessage::m_DDFDescriptor->m_Name, result);
        }
    }

    void RunPhysicsCallback(CollisionWorld* world, const dmDDF::Descriptor* desc, const char* data)
    {
        RunCollisionWorldCallback(world->m_CallbackInfo, desc, data);
    }

    bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        CollisionUserData* cud = (CollisionUserData*)user_data;
        if (cud->m_Count < cud->m_Context->m_MaxCollisionCount)
        {
            CollisionWorld* world = cud->m_World;
            cud->m_Count += 1;

            CollisionComponent* component_a = (CollisionComponent*)user_data_a;
            CollisionComponent* component_b = (CollisionComponent*)user_data_b;
            dmGameObject::HInstance instance_a = component_a->m_Instance;
            dmGameObject::HInstance instance_b = component_b->m_Instance;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);
            uint64_t group_hash_a = GetLSBGroupHash(world, group_a);
            uint64_t group_hash_b = GetLSBGroupHash(world, group_b);

            if (world->m_CallbackInfo != 0x0)
            {
                dmPhysicsDDF::CollisionEvent ddf;

                dmPhysicsDDF::Collision& a = ddf.m_A;
                a.m_Group =     group_hash_a;
                a.m_Id =        instance_a_id;
                a.m_Position =  dmGameObject::GetWorldPosition(instance_a);

                dmPhysicsDDF::Collision& b = ddf.m_B;
                b.m_Group =     group_hash_b;
                b.m_Id =        instance_b_id;
                b.m_Position =  dmGameObject::GetWorldPosition(instance_b);

                RunPhysicsCallback(world, dmPhysicsDDF::CollisionEvent::m_DDFDescriptor, (const char*)&ddf);
                return true;
            }

            dmPhysicsDDF::CollisionResponse ddf;

            // Broadcast to A components
            ddf.m_OwnGroup = group_hash_a;
            ddf.m_OtherGroup = group_hash_b;
            ddf.m_Group = group_hash_b;
            ddf.m_OtherId = instance_b_id;
            ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_b);
            BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);

            // Broadcast to B components
            ddf.m_OwnGroup = group_hash_b;
            ddf.m_OtherGroup = group_hash_a;
            ddf.m_Group = group_hash_a;
            ddf.m_OtherId = instance_a_id;
            ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_a);
            BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);

            return true;
        }
        else
        {
            return false;
        }
    }

    bool ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
    {
        CollisionUserData* cud = (CollisionUserData*)user_data;
        if (cud->m_Count < cud->m_Context->m_MaxContactPointCount)
        {
            CollisionWorld* world = cud->m_World;
            cud->m_Count += 1;

            CollisionComponent* component_a = (CollisionComponent*)contact_point.m_UserDataA;
            CollisionComponent* component_b = (CollisionComponent*)contact_point.m_UserDataB;
            dmGameObject::HInstance instance_a = component_a->m_Instance;
            dmGameObject::HInstance instance_b = component_b->m_Instance;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);
            float mass_a = dmMath::Select(-contact_point.m_MassA, 0.0f, contact_point.m_MassA);
            float mass_b = dmMath::Select(-contact_point.m_MassB, 0.0f, contact_point.m_MassB);
            uint64_t group_hash_a = GetLSBGroupHash(world, contact_point.m_GroupA);
            uint64_t group_hash_b = GetLSBGroupHash(world, contact_point.m_GroupB);

            if (world->m_CallbackInfo != 0x0)
            {
                dmPhysicsDDF::ContactPointEvent ddf;
                ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
                ddf.m_Distance = contact_point.m_Distance;

                dmPhysicsDDF::ContactPoint& a = ddf.m_A;
                a.m_Group               = group_hash_a;
                a.m_Id                  = instance_a_id;
                a.m_Position            = dmGameObject::GetWorldPosition(instance_a);
                a.m_Mass                = mass_a;
                a.m_RelativeVelocity    = -contact_point.m_RelativeVelocity;
                a.m_Normal              = -contact_point.m_Normal;

                dmPhysicsDDF::ContactPoint& b = ddf.m_B;
                b.m_Group               = group_hash_b;
                b.m_Id                  = instance_b_id;
                b.m_Position            = dmGameObject::GetWorldPosition(instance_b);
                b.m_Mass                = mass_b;
                b.m_RelativeVelocity    = contact_point.m_RelativeVelocity;
                b.m_Normal              = contact_point.m_Normal;

                RunPhysicsCallback(world, dmPhysicsDDF::ContactPointEvent::m_DDFDescriptor, (const char*)&ddf);
                return true;
            }

            dmPhysicsDDF::ContactPointResponse ddf;

            // Broadcast to A components
            ddf.m_Position = contact_point.m_PositionA;
            ddf.m_Normal = -contact_point.m_Normal;
            ddf.m_RelativeVelocity = -contact_point.m_RelativeVelocity;
            ddf.m_Distance = contact_point.m_Distance;
            ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
            ddf.m_Mass = mass_a;
            ddf.m_OtherMass = mass_b;
            ddf.m_OtherId = instance_b_id;
            ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_b);
            ddf.m_Group = group_hash_b;
            ddf.m_OwnGroup = group_hash_a;
            ddf.m_OtherGroup = group_hash_b;
            ddf.m_LifeTime = 0;
            BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);

            // Broadcast to B components
            ddf.m_Position = contact_point.m_PositionB;
            ddf.m_Normal = contact_point.m_Normal;
            ddf.m_RelativeVelocity = contact_point.m_RelativeVelocity;
            ddf.m_Distance = contact_point.m_Distance;
            ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
            ddf.m_Mass = mass_b;
            ddf.m_OtherMass = mass_a;
            ddf.m_OtherId = instance_a_id;
            ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_a);
            ddf.m_Group = group_hash_a;
            ddf.m_OwnGroup = group_hash_b;
            ddf.m_OtherGroup = group_hash_a;
            ddf.m_LifeTime = 0;
            BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);

            return true;
        }
        else
        {
            return false;
        }
    }

    static bool g_CollisionOverflowWarning = false;
    static bool g_ContactOverflowWarning = false;

    void TriggerEnteredCallback(const dmPhysics::TriggerEnter& trigger_enter, void* user_data)
    {
        CollisionWorld* world = (CollisionWorld*)user_data;
        CollisionComponent* component_a = (CollisionComponent*)trigger_enter.m_UserDataA;
        CollisionComponent* component_b = (CollisionComponent*)trigger_enter.m_UserDataB;
        dmGameObject::HInstance instance_a = component_a->m_Instance;
        dmGameObject::HInstance instance_b = component_b->m_Instance;
        dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
        dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);

        uint64_t group_hash_a = GetLSBGroupHash(world, trigger_enter.m_GroupA);
        uint64_t group_hash_b = GetLSBGroupHash(world, trigger_enter.m_GroupB);

        if (world->m_CallbackInfo != 0x0)
        {

            dmPhysicsDDF::TriggerEvent ddf;
            ddf.m_Enter = 1;

            dmPhysicsDDF::Trigger& a = ddf.m_A;
            a.m_Group       = group_hash_a;
            a.m_Id          = instance_a_id;

            dmPhysicsDDF::Trigger& b = ddf.m_B;
            b.m_Group       = group_hash_b;
            b.m_Id          = instance_b_id;

            RunPhysicsCallback(world, dmPhysicsDDF::TriggerEvent::m_DDFDescriptor, (const char*)&ddf);
            return;
        }

        dmPhysicsDDF::TriggerResponse ddf;
        ddf.m_Enter = 1;

        // Broadcast to A components
        ddf.m_OtherId = instance_b_id;
        ddf.m_Group = group_hash_b;
        ddf.m_OwnGroup = group_hash_a;
        ddf.m_OtherGroup = group_hash_b;
        BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);

        // Broadcast to B components
        ddf.m_OtherId = instance_a_id;
        ddf.m_Group = group_hash_a;
        ddf.m_OwnGroup = group_hash_b;
        ddf.m_OtherGroup = group_hash_a;
        BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
    }

    void TriggerExitedCallback(const dmPhysics::TriggerExit& trigger_exit, void* user_data)
    {
        CollisionWorld* world = (CollisionWorld*)user_data;
        CollisionComponent* component_a = (CollisionComponent*)trigger_exit.m_UserDataA;
        CollisionComponent* component_b = (CollisionComponent*)trigger_exit.m_UserDataB;
        dmGameObject::HInstance instance_a = component_a->m_Instance;
        dmGameObject::HInstance instance_b = component_b->m_Instance;
        dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
        dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);

        uint64_t group_hash_a = GetLSBGroupHash(world, trigger_exit.m_GroupA);
        uint64_t group_hash_b = GetLSBGroupHash(world, trigger_exit.m_GroupB);

        if (world->m_CallbackInfo != 0x0)
        {
            dmPhysicsDDF::TriggerEvent ddf;
            ddf.m_Enter = 0;

            dmPhysicsDDF::Trigger& a = ddf.m_A;
            a.m_Group       = group_hash_a;
            a.m_Id          = instance_a_id;

            dmPhysicsDDF::Trigger& b = ddf.m_B;
            b.m_Group       = group_hash_b;
            b.m_Id          = instance_b_id;

            RunPhysicsCallback(world, dmPhysicsDDF::TriggerEvent::m_DDFDescriptor, (const char*)&ddf);
            return;
        }

        dmPhysicsDDF::TriggerResponse ddf;
        ddf.m_Enter = 0;

        // Broadcast to A components
        ddf.m_OtherId = instance_b_id;
        ddf.m_Group = group_hash_b;
        ddf.m_OwnGroup = group_hash_a;
        ddf.m_OtherGroup = group_hash_b;
        BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);

        // Broadcast to B components
        ddf.m_OtherId = instance_a_id;
        ddf.m_Group = group_hash_a;
        ddf.m_OwnGroup = group_hash_b;
        ddf.m_OtherGroup = group_hash_a;
        BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
    }

    struct RayCastUserData
    {
        dmGameObject::HInstance m_Instance;
        CollisionWorld* m_World;
    };

    static void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request, void* user_data)
    {
        dmGameObject::Result message_result = dmGameObject::RESULT_OK;
        CollisionWorld* world = (CollisionWorld*)user_data;
        if (response.m_Hit)
        {
            dmPhysicsDDF::RayCastResponse response_ddf;
            CollisionComponent* component = (CollisionComponent*)response.m_CollisionObjectUserData;

            response_ddf.m_Fraction = response.m_Fraction;
            response_ddf.m_Id = dmGameObject::GetIdentifier(component->m_Instance);
            response_ddf.m_Group = GetLSBGroupHash(world, response.m_CollisionObjectGroup);
            response_ddf.m_Position = response.m_Position;
            response_ddf.m_Normal = response.m_Normal;
            response_ddf.m_RequestId = request.m_UserId & 0xff;

            if (world->m_CallbackInfo != 0x0)
            {
                RunPhysicsCallback(world, dmPhysicsDDF::RayCastResponse::m_DDFDescriptor, (const char*)&response_ddf);
            }
            else
            {
                message_result = dmGameObject::PostDDF(&response_ddf, 0x0, (dmMessage::URL*)request.m_UserData, 0x0, false);
            }
        }
        else
        {
            dmPhysicsDDF::RayCastMissed missed_ddf;
            missed_ddf.m_RequestId = request.m_UserId & 0xff;
            if (world->m_CallbackInfo != 0x0)
            {
                RunPhysicsCallback(world, dmPhysicsDDF::RayCastMissed::m_DDFDescriptor, (const char*)&missed_ddf);
            }
            else
            {
                message_result = dmGameObject::PostDDF(&missed_ddf, 0x0, (dmMessage::URL*)request.m_UserData, 0x0, false);
            }
        }
        free(request.m_UserData);
        if (message_result != dmGameObject::RESULT_OK)
        {
            dmLogError("Error when sending ray cast response: %d", message_result);
        }
    }

    struct DispatchContext
    {
        PhysicsContext* m_PhysicsContext;
        bool m_Success;
        dmGameObject::HCollection m_Collection;
        CollisionWorld* m_World;
    };

    void DispatchCallback(dmMessage::Message *message, void* user_ptr)
    {
        DispatchContext* context = (DispatchContext*)user_ptr;
        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
            if (descriptor == dmPhysicsDDF::RequestRayCast::m_DDFDescriptor)
            {
                dmPhysicsDDF::RequestRayCast* ddf = (dmPhysicsDDF::RequestRayCast*)message->m_Data;
                dmGameObject::HInstance sender_instance = (dmGameObject::HInstance)message->m_UserData1;
                uint16_t component_index;
                dmGameObject::Result go_result = dmGameObject::GetComponentIndex(sender_instance, message->m_Sender.m_Fragment, &component_index);
                if (go_result != dmGameObject::RESULT_OK)
                {
                    dmLogError("Component index could not be retrieved when handling '%s': %d.", dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_Name, go_result);
                    context->m_Success = false;
                }
                else
                {
                    // Target collection which can be different than we are updating for.
                    dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

                    // NOTE! The collision world for the target collection is looked up using this worlds component index
                    //       which is assumed to be the same as in the target collection.
                    CollisionWorld* world = (CollisionWorld*) dmGameObject::GetWorld(collection, context->m_World->m_ComponentTypeIndex);

                    // Give that the assumption above holds, this assert will hold too.
                    assert(world->m_ComponentTypeIndex == context->m_World->m_ComponentTypeIndex);

                    dmMessage::URL* receiver = (dmMessage::URL*)malloc(sizeof(dmMessage::URL));
                    dmMessage::ResetURL(receiver);
                    receiver->m_Socket = dmGameObject::GetMessageSocket(collection);
                    receiver->m_Path = dmGameObject::GetIdentifier(sender_instance);

                    dmPhysics::RayCastRequest request;
                    request.m_From = ddf->m_From;
                    request.m_To = ddf->m_To;
                    request.m_IgnoredUserData = sender_instance;
                    request.m_Mask = ddf->m_Mask;
                    request.m_UserId = component_index << 16 | (ddf->m_RequestId & 0xff);
                    request.m_UserData = (void*)receiver;
                    if (world->m_3D)
                    {
                        dmPhysics::RequestRayCast3D(world->m_World3D, request);
                    }
                    else
                    {
                        dmPhysics::RequestRayCast2D(world->m_World2D, request);
                    }
                }
            }
        }
    }

    dmGameObject::CreateResult CompCollisionObjectAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        CollisionWorld* world = (CollisionWorld*)params.m_World;
        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if (world->m_Components.Full())
        {
            ShowFullBufferError("Collision object", PHYSICS_MAX_COLLISION_OBJECTS_KEY, world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        assert(!component->m_AddedToUpdate);

        if (component->m_3D) {
            dmPhysics::SetEnabled3D(world->m_World3D, component->m_Object3D, component->m_StartAsEnabled);
        } else {
            dmPhysics::SetEnabled2D(world->m_World2D, component->m_Object2D, component->m_StartAsEnabled);
        }
        component->m_AddedToUpdate = true;

        world->m_Components.Push(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    // This function will dispatch on the (global) physics socket, and will potentially handle messages belonging to other collections
    // than the current one being updated.
    //
    // TODO: Make a nicer solution for this, perhaps a per-collection physics socket
    bool CompCollisionObjectDispatchPhysicsMessages(PhysicsContext *physics_context, CollisionWorld *world, dmGameObject::HCollection collection)
    {
        DispatchContext dispatch_context;
        dispatch_context.m_PhysicsContext = physics_context;
        dispatch_context.m_Success = true;
        dispatch_context.m_World = world;
        dispatch_context.m_Collection = collection;
        dmMessage::HSocket physics_socket;
        if (physics_context->m_3D)
        {
            physics_socket = dmPhysics::GetSocket3D(physics_context->m_Context3D);
        }
        else
        {
            physics_socket = dmPhysics::GetSocket2D(physics_context->m_Context2D);

        }
        dmMessage::Dispatch(physics_socket, DispatchCallback, (void*)&dispatch_context);
        return dispatch_context.m_Success;
    }

    static void Step(CollisionWorld* world, PhysicsContext* physics_context, dmGameObject::HCollection collection, const dmPhysics::StepWorldContext* step_ctx)
    {
        CollisionUserData* collision_user_data = (CollisionUserData*)step_ctx->m_CollisionUserData;
        CollisionUserData* contact_user_data = (CollisionUserData*)step_ctx->m_ContactPointUserData;

        g_NumPhysicsTransformsUpdated = 0;

        world->m_CurrentDT = step_ctx->m_DT;

        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, collection))
        {
            dmLogWarning("Failed to dispatch physics messages");
        }

        if (physics_context->m_3D)
        {
            DM_PROFILE("StepWorld3D");
            dmPhysics::StepWorld3D(world->m_World3D, *step_ctx);
        }
        else
        {
            DM_PROFILE("StepWorld2D");
            dmPhysics::StepWorld2D(world->m_World2D, *step_ctx);
        }

        if (collision_user_data->m_Count >= physics_context->m_MaxCollisionCount)
        {
            if (!g_CollisionOverflowWarning)
            {
                dmLogWarning("Maximum number of collisions (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_MaxCollisionCount, PHYSICS_MAX_COLLISIONS_KEY);
                g_CollisionOverflowWarning = true;
            }
        }
        else
        {
            g_CollisionOverflowWarning = false;
        }
        if (contact_user_data->m_Count >= physics_context->m_MaxContactPointCount)
        {
            if (!g_ContactOverflowWarning)
            {
                dmLogWarning("Maximum number of contacts (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_MaxContactPointCount, PHYSICS_MAX_CONTACTS_KEY);
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

        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionWorld* world = (CollisionWorld*)params.m_World;

        // Hot-reload is not available in release, so lets not iterate collision components in that case.
        if (dLib::IsDebugMode())
        {
            uint32_t num_components = world->m_Components.Size();
            for (uint32_t i = 0; i < num_components; ++i)
            {
                CollisionComponent* c = world->m_Components[i];
                TileGridResource* tile_grid_res = c->m_Resource->m_TileGridResource;
                if (tile_grid_res != 0x0 && tile_grid_res->m_Dirty)
                {
                    CollisionObjectResource* resource = c->m_Resource;
                    dmPhysicsDDF::CollisionObjectDesc* ddf = resource->m_DDF;
                    dmPhysics::CollisionObjectData data;
                    SetCollisionObjectData(world, c, c->m_Resource, ddf, true, data);
                    c->m_Mask = data.m_Mask;

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
        collision_user_data.m_World = world;
        collision_user_data.m_Context = physics_context;
        collision_user_data.m_Count = 0;
        CollisionUserData contact_user_data;
        contact_user_data.m_World = world;
        contact_user_data.m_Context = physics_context;
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
        step_world_context.m_FixedTimeStep = physics_context->m_UseFixedTimestep;
        step_world_context.m_MaxFixedTimeSteps = physics_context->m_MaxFixedTimesteps;

        step_world_context.m_DT = params.m_UpdateContext->m_DT;

        Step(world, physics_context, params.m_Collection, &step_world_context);

        // We only want to call this once per frame
        if (physics_context->m_3D)
            dmPhysics::SetDrawDebug3D(world->m_World3D, physics_context->m_Debug);
        else
            dmPhysics::SetDrawDebug2D(world->m_World2D, physics_context->m_Debug);

        DM_PROPERTY_ADD_U32(rmtp_CollisionObject, world->m_Components.Size());
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {

        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (physics_context->m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the fixed update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }

    dmGameObject::UpdateResult CompCollisionObjectFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (!physics_context->m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the dynamic update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }

    dmGameObject::UpdateResult CompCollisionObjectPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;

        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionWorld* world = (CollisionWorld*)params.m_World;

        // Dispatch also in post-messages since messages might have been posting from script components, or init
        // functions in factories, and they should not linger around to next frame (which might not come around)
        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, params.m_Collection))
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionComponent* component = (CollisionComponent*) *params.m_UserData;

        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash
                || params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            bool enable = false;
            if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
            {
                enable = true;
            }
            CollisionWorld* world = (CollisionWorld*)params.m_World;

            if (component->m_AddedToUpdate)
            {
                if (physics_context->m_3D)
                {
                    dmPhysics::SetEnabled3D(world->m_World3D, component->m_Object3D, enable);
                }
                else
                {
                    dmPhysics::SetEnabled2D(world->m_World2D, component->m_Object2D, enable);
                }
            }
            else
            {
                // Deferred controlling the enabled state. Objects are force disabled until
                // they are added to update.
                component->m_StartAsEnabled = enable;
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::ApplyForce::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::ApplyForce* af = (dmPhysicsDDF::ApplyForce*) params.m_Message->m_Data;
            if (physics_context->m_3D)
            {
                dmPhysics::ApplyForce3D(physics_context->m_Context3D, component->m_Object3D, af->m_Force, af->m_Position);
            }
            else
            {
                dmPhysics::ApplyForce2D(physics_context->m_Context2D, component->m_Object2D, af->m_Force, af->m_Position);
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::RequestVelocity::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::VelocityResponse response;
            if (physics_context->m_3D)
            {
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity3D(physics_context->m_Context3D, component->m_Object3D);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity3D(physics_context->m_Context3D, component->m_Object3D);
            }
            else
            {
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity2D(physics_context->m_Context2D, component->m_Object2D);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity2D(physics_context->m_Context2D, component->m_Object2D);
            }
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
            if (physics_context->m_3D)
            {
                dmLogError("Grid shape hulls can only be set for 2D physics.");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            if (component->m_Resource->m_TileGrid == 0)
            {
                dmLogError("Hulls can only be set for collision objects with tile grids as shape.");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            dmPhysicsDDF::SetGridShapeHull* ddf = (dmPhysicsDDF::SetGridShapeHull*) params.m_Message->m_Data;
            uint32_t column = ddf->m_Column;
            uint32_t row = ddf->m_Row;
            uint32_t hull = ddf->m_Hull;

            TileGridResource* tile_grid_resource = component->m_Resource->m_TileGridResource;

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
            uint16_t child = column + tile_grid_resource->m_ColumnCount * row;
            uint16_t group = 0;
            uint16_t mask = 0;
            // Hull-index of 0xffffffff is empty cell
            if (hull != ~0u)
            {
                group = GetGroupBitIndex((CollisionWorld*)params.m_World, tile_grid_resource->m_TextureSet->m_HullCollisionGroups[hull], false);
                mask = component->m_Mask;
            }
            dmPhysics::SetCollisionObjectFilter(component->m_Object2D, ddf->m_Shape, child, group, mask);
        }
        else if(params.m_Message->m_Id == dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor->m_NameHash)
        {
            assert(!physics_context->m_3D);
            if (component->m_Resource->m_TileGrid == 0)
            {
                dmLogError("Layer visibility can only be set on tile grids");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            dmPhysicsDDF::EnableGridShapeLayer* ddf = (dmPhysicsDDF::EnableGridShapeLayer*) params.m_Message->m_Data;
            dmPhysics::SetGridShapeEnable(component->m_Object2D, ddf->m_Shape, ddf->m_Enable);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionWorld* world = (CollisionWorld*)params.m_World;
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        component->m_Resource = (CollisionObjectResource*)params.m_Resource;
        component->m_AddedToUpdate = false;
        component->m_StartAsEnabled = true;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, true))
        {
            dmLogError("%s", "Could not recreate collision object component, not reloaded.");
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value) {
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (params.m_PropertyId == PROP_LINEAR_VELOCITY) {
            if (physics_context->m_3D) {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearVelocity3D(physics_context->m_Context3D, component->m_Object3D));
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearVelocity2D(physics_context->m_Context2D, component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY) {
            if (physics_context->m_3D) {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularVelocity3D(physics_context->m_Context3D, component->m_Object3D));
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularVelocity2D(physics_context->m_Context2D, component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_MASS) {
            if (physics_context->m_3D) {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetMass3D(component->m_Object3D));
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetMass2D(component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_BULLET) {
            if (physics_context->m_3D) {
                dmLogWarning("'bullet' property not supported in 3d physics mode");
                return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::IsBullet2D(component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_LINEAR_DAMPING) {
            if (physics_context->m_3D) {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearDamping3D(component->m_Object3D));
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearDamping2D(component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else if (params.m_PropertyId == PROP_ANGULAR_DAMPING) {
            if (physics_context->m_3D) {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularDamping3D(component->m_Object3D));
            } else {
                out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularDamping2D(component->m_Object2D));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        } else {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectSetProperty(const dmGameObject::ComponentSetPropertyParams& params) {
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;

        if (params.m_PropertyId == PROP_LINEAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            if (physics_context->m_3D)
            {
                dmPhysics::SetLinearVelocity3D(physics_context->m_Context3D, component->m_Object3D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            }
            else
            {
                dmPhysics::SetLinearVelocity2D(physics_context->m_Context2D, component->m_Object2D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            if (physics_context->m_3D)
            {
                dmPhysics::SetAngularVelocity3D(physics_context->m_Context3D, component->m_Object3D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            }
            else
            {
                dmPhysics::SetAngularVelocity2D(physics_context->m_Context2D, component->m_Object2D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_BULLET)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            if (physics_context->m_3D)
            {
                dmLogWarning("'bullet' property not supported in 3d physics mode");
                return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
            }
            else
            {
                dmPhysics::SetBullet2D(component->m_Object2D, params.m_Value.m_Bool);
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_LINEAR_DAMPING) {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            if (physics_context->m_3D)
            {
                dmPhysics::SetLinearDamping3D(component->m_Object3D, params.m_Value.m_Number);
            }
            else
            {
                dmPhysics::SetLinearDamping2D(component->m_Object2D, params.m_Value.m_Number);
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_DAMPING)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            if (physics_context->m_3D)
            {
                dmPhysics::SetAngularDamping3D(component->m_Object3D, params.m_Value.m_Number);
            }
            else
            {
                dmPhysics::SetAngularDamping2D(component->m_Object2D, params.m_Value.m_Number);
            }
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

    uint16_t CompCollisionGetGroupBitIndex(void* world, uint64_t group_hash)
    {
        return GetGroupBitIndex((CollisionWorld*)world, group_hash, false);
    }

    void RayCast(void* _world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (world->m_3D)
        {
            dmPhysics::RayCast3D(world->m_World3D, request, results);
        }
        else
        {
            dmPhysics::RayCast2D(world->m_World2D, request, results);
        }
    }

    // Find a JointEntry in the linked list of a collision component based on the joint id.
    static JointEntry* FindJointEntry(CollisionWorld* world, CollisionComponent* component, dmhash_t id)
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

    inline bool IsJointsSupported(CollisionWorld* world)
    {
        if (world->m_3D)
        {
            dmLogError("joints are currently only available in 2D physics");
            return false;
        }
        return true;
    }

    // Connects a joint between two components, a JointEntry with the id must exist for this to succeed.
    dmPhysics::JointResult CreateJoint(void* _world, void* _component_a, dmhash_t id, const dmVMath::Point3& apos, void* _component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        if (dmPhysics::IsWorldLocked(world->m_World2D)) {
            return dmPhysics::RESULT_PHYSICS_WORLD_LOCKED;
        }

        CollisionComponent* component_a = (CollisionComponent*)_component_a;
        CollisionComponent* component_b = (CollisionComponent*)_component_b;

        // Check if there is already a joint with this id
        JointEntry* joint_entry = FindJointEntry(world, component_a, id);
        if (joint_entry) {
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

    dmPhysics::JointResult DestroyJoint(void* _world, void* _component, dmhash_t id)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);
        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        DeleteJoint(world, joint_entry);

        // Remove joint entry from list on component instance
        if (component->m_Joints == joint_entry) {
            component->m_Joints = joint_entry->m_Next;
        } else {
            JointEntry* j = component->m_Joints;
            while (j) {
                if (j->m_Next == joint_entry) {
                    j->m_Next = joint_entry->m_Next;
                    break;
                }
                j = j->m_Next;
            }
        }

        delete joint_entry;

        return dmPhysics::RESULT_OK;
    }

    dmPhysics::JointResult GetJointParams(void* _world, void* _component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
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

    dmPhysics::JointResult GetJointType(void* _world, void* _component, dmhash_t id, dmPhysics::JointType& joint_type)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
        JointEntry* joint_entry = FindJointEntry(world, component, id);

        if (!joint_entry) {
            return dmPhysics::RESULT_ID_NOT_FOUND;
        }

        if (!joint_entry->m_Joint) {
            return dmPhysics::RESULT_NOT_CONNECTED;
        }

        joint_type = joint_entry->m_Type;

        return dmPhysics::RESULT_OK;
    }

    dmPhysics::JointResult SetJointParams(void* _world, void* _component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
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

    static void DeleteJoint(CollisionWorld* world, JointEntry* joint_entry)
    {
        assert(joint_entry);

        DeleteJoint(world, joint_entry->m_Joint);
        joint_entry->m_Joint = 0x0;

        // Remove end point
        assert(joint_entry->m_EndPoint);
        JointEndPoint* end_point = joint_entry->m_EndPoint;
        CollisionComponent* owner_component = end_point->m_Owner;

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

    static void DeleteJoint(CollisionWorld* world, dmPhysics::HJoint joint)
    {
        assert(joint);
        if (!world->m_3D)
        {
            dmPhysics::DeleteJoint2D(world->m_World2D, joint);
        }
    }

    dmPhysics::JointResult GetJointReactionForce(void* _world, void* _component, dmhash_t id, dmVMath::Vector3& force)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
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

    dmPhysics::JointResult GetJointReactionTorque(void* _world, void* _component, dmhash_t id, float& torque)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (!IsJointsSupported(world)) {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }

        CollisionComponent* component = (CollisionComponent*)_component;
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

    void SetGravity(void* _world, const dmVMath::Vector3& gravity)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (world->m_3D)
        {
            dmPhysics::SetGravity3D(world->m_World3D, gravity);
        }
        else
        {
            dmPhysics::SetGravity2D(world->m_World2D, gravity);
        }
    }

    dmVMath::Vector3 GetGravity(void* _world)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (world->m_3D)
        {
            return dmPhysics::GetGravity3D(world->m_World3D);
        }
        else
        {
            return dmPhysics::GetGravity2D(world->m_World2D);
        }
    }

    bool GetShapeIndex(void* _component, dmhash_t shape_name_hash, uint32_t* index_out)
    {
        CollisionComponent* component = (CollisionComponent*) _component;
        uint32_t shape_count = component->m_Resource->m_DDF->m_EmbeddedCollisionShape.m_Shapes.m_Count;
        for (int i = 0; i < shape_count; ++i)
        {
            if (component->m_Resource->m_DDF->m_EmbeddedCollisionShape.m_Shapes[i].m_IdHash == shape_name_hash)
            {
                *index_out = i;
                return true;
            }
        }
        return false;
    }

    static inline dmPhysics::HCollisionShape3D GetShape3D(CollisionComponent* component, uint32_t shape_index)
    {
        if (component->m_ShapeBuffer)
            return component->m_ShapeBuffer[shape_index];
        return dmPhysics::GetCollisionShape3D(component->m_Object3D, shape_index);
    }

    bool GetShape(void* _world, void* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionWorld* world         = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*) _component;
        uint32_t shape_count          = component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        shape_info->m_Type = component->m_Resource->m_ShapeTypes[shape_ix];

        if (world->m_3D)
        {
            dmPhysics::HCollisionShape3D shape3d = GetShape3D(component, shape_ix);

            switch(shape_info->m_Type)
            {
                case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
                {
                    float sphere_radius;
                    dmPhysics::GetCollisionShapeRadius3D(shape3d, &sphere_radius);
                    shape_info->m_SphereDiameter = sphere_radius * 2.0f;
                } break;
                case dmPhysicsDDF::CollisionShape::TYPE_BOX:
                {
                    float half_extents[3];
                    dmPhysics::GetCollisionShapeHalfBoxExtents3D(shape3d, half_extents);
                    shape_info->m_BoxDimensions[0] = half_extents[0] * 2.0f;
                    shape_info->m_BoxDimensions[1] = half_extents[1] * 2.0f;
                    shape_info->m_BoxDimensions[2] = half_extents[2] * 2.0f;
                } break;
                case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
                {
                    float radius, half_height;
                    dmPhysics::GetCollisionShapeCapsuleRadiusHeight3D(shape3d, &radius, &half_height);
                    shape_info->m_CapsuleDiameterHeight[0] = radius * 2.0f;
                    shape_info->m_CapsuleDiameterHeight[1] = half_height * 2.0f;
                } break;
                default: assert(0);
            }
        }
        else
        {
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
                    dmPhysics::GetCollisionShapeBoxDimensions2D(world->m_World2D, shape2d, component->m_Resource->m_ShapeRotation[shape_ix], shape_info->m_BoxDimensions[0], shape_info->m_BoxDimensions[1]);
                } break;
                default: assert(0);
            }
        }
        return true;
    }

    static void ReplaceAndDeleteShape3D(dmPhysics::HContext3D context,
        CollisionComponent* component,
        dmPhysics::HCollisionShape3D old_shape,
        dmPhysics::HCollisionShape3D new_shape,
        uint32_t shape_index)
    {
        uint32_t shape_count = component->m_Resource->m_ShapeCount;

        // The direction table is needed for component that has a box shape,
        // because it is the only shape that currently needs to alter the shape hierarchy
        // of a collision object.
        if (!component->m_ShapeBuffer)
        {
            component->m_ShapeBuffer = new dmPhysics::HCollisionShape3D[shape_count];
            uint32_t res = dmPhysics::GetCollisionShapes3D(component->m_Object3D, component->m_ShapeBuffer, shape_count);
            assert(res == shape_count);
        }

        dmPhysics::ReplaceShape3D(context, old_shape, new_shape);
        dmPhysics::ReplaceShape3D(component->m_Object3D, old_shape, new_shape);
        dmPhysics::DeleteCollisionShape3D(old_shape);

        component->m_ShapeBuffer[shape_index] = new_shape;
    }

    bool SetShape(void* _world, void* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionWorld* world         = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*) _component;
        uint32_t shape_count          = component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        if (world->m_3D)
        {
            dmPhysics::HCollisionShape3D shape3d = GetShape3D(component, shape_ix);

            switch(shape_info->m_Type)
            {
                case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
                {
                    dmPhysics::SetCollisionShapeRadius3D(shape3d, shape_info->m_SphereDiameter * 0.5f);
                } break;
                case dmPhysicsDDF::CollisionShape::TYPE_BOX:
                {
                    dmPhysics::HCollisionShape3D new_shape = dmPhysics::NewBoxShape3D(dmPhysics::GetContext3D(world->m_World3D),
                        dmVMath::Vector3(shape_info->m_BoxDimensions[0] * 0.5f,
                                         shape_info->m_BoxDimensions[1] * 0.5f,
                                         shape_info->m_BoxDimensions[2] * 0.5f));

                    ReplaceAndDeleteShape3D(dmPhysics::GetContext3D(world->m_World3D), component, shape3d, new_shape, shape_ix);
                } break;
                case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
                {
                    dmPhysics::HCollisionShape3D new_shape = dmPhysics::NewCapsuleShape3D(dmPhysics::GetContext3D(world->m_World3D),
                        shape_info->m_CapsuleDiameterHeight[0] * 0.5f,
                        shape_info->m_CapsuleDiameterHeight[1]);

                    ReplaceAndDeleteShape3D(dmPhysics::GetContext3D(world->m_World3D), component, shape3d, new_shape, shape_ix);
                } break;
                default: assert(0);
            }
        }
        else
        {
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
                    dmPhysics::SetCollisionShapeBoxDimensions2D(world->m_World2D, shape2d, component->m_Resource->m_ShapeRotation[shape_ix], shape_info->m_BoxDimensions[0] * 0.5f, shape_info->m_BoxDimensions[1] * 0.5f);
                } break;
                default: assert(0);
            }
        }

        return true;
    }

    dmhash_t CompCollisionObjectGetIdentifier(void* _component)
    {
        CollisionComponent* component = (CollisionComponent*)_component;
        return dmGameObject::GetIdentifier(component->m_Instance);
    }

    bool IsCollision2D(void* _world)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        return !world->m_3D;
    }

    void SetCollisionFlipH(void* _component, bool flip)
    {
        CollisionComponent* component = (CollisionComponent*)_component;
        if (component->m_FlippedX != flip)
            dmPhysics::FlipH2D(component->m_Object2D);
        component->m_FlippedX = flip;
    }

    void SetCollisionFlipV(void* _component, bool flip)
    {
        CollisionComponent* component = (CollisionComponent*)_component;
        if (component->m_FlippedY != flip)
            dmPhysics::FlipV2D(component->m_Object2D);
        component->m_FlippedY = flip;
    }

    void WakeupCollision(void* _world, void* _component)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        if (world->m_3D)
        {
            dmPhysics::Wakeup3D(component->m_Object3D);
        } else
        {
            dmPhysics::Wakeup2D(component->m_Object2D);
        }
    }

    void* GetCollisionWorldCallback(void* _world)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        return world->m_CallbackInfo;
    }

    void SetCollisionWorldCallback(void* _world, void* callback_info)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        world->m_CallbackInfo = callback_info;
    }

    dmhash_t GetCollisionGroup(void* _world, void* _component)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        uint16_t groupbit;
        if (world->m_3D)
        {
            groupbit = dmPhysics::GetGroup3D(component->m_Object3D);
        } else
        {
            groupbit = dmPhysics::GetGroup2D(component->m_Object2D);
        }
        return GetLSBGroupHash(world, groupbit);
    }

    // returns false if no such collision group has been registered
    bool SetCollisionGroup(void* _world, void* _component, dmhash_t group_hash)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        uint16_t groupbit = GetGroupBitIndex(world, group_hash, true);
        if (!groupbit)
        {
            return false; // error. No such group.
        }

        if (world->m_3D)
        {
            dmPhysics::SetGroup3D(world->m_World3D, component->m_Object3D, groupbit);
        } else
        {
            dmPhysics::SetGroup2D(component->m_Object2D, groupbit);
        }
        return true; // all good
    }

    // Updates 'maskbit' with the mask value. Returns false if no such collision group has been registered.
    bool GetCollisionMaskBit(void* _world, void* _component, dmhash_t group_hash, bool* maskbit)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        uint16_t groupbit = GetGroupBitIndex(world, group_hash, true);
        if (!groupbit) {
            return false;
        }

        if (world->m_3D)
        {
            *maskbit = dmPhysics::GetMaskBit3D(component->m_Object3D, groupbit);
        } else
        {
            *maskbit = dmPhysics::GetMaskBit2D(component->m_Object2D, groupbit);
        }
        return true;
	}

    // returns false if no such collision group has been registered
    bool SetCollisionMaskBit(void* _world, void* _component, dmhash_t group_hash, bool boolvalue)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        uint16_t groupbit = GetGroupBitIndex(world, group_hash, true);
        if (!groupbit)
        {
            return false;
        }

        if (world->m_3D)
        {
            dmPhysics::SetMaskBit3D(world->m_World3D, component->m_Object3D, groupbit, boolvalue);
        } else
        {
            dmPhysics::SetMaskBit2D(component->m_Object2D, groupbit, boolvalue);
        }
        return true;
    }

    void UpdateMass(void* _world, void* _component, float mass)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        CollisionComponent* component = (CollisionComponent*)_component;

        if (world->m_3D)
        {
            dmLogError("The Update Mass function has not been implemented for 3D yet");
        }
        else
        {
            if(!dmPhysics::UpdateMass2D(component->m_Object2D, mass))
            {
                dmLogError("The Update Mass function can be used only for Dynamic objects with shape area > 0");
            }
        }
    }

    static bool CompCollisionIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        CollisionWorld* world = (CollisionWorld*)pit->m_Node->m_ComponentWorld;
        CollisionComponent* component = (CollisionComponent*)pit->m_Node->m_Component;

        uint64_t index = pit->m_Next++;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                if (world->m_3D)
                {
                    pit->m_Property.m_Value.m_Bool = dmPhysics::IsEnabled3D(component->m_Object3D);
                }
                else
                {
                    pit->m_Property.m_Value.m_Bool = dmPhysics::IsEnabled2D(component->m_Object2D);
                }
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    void CompCollisionIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompCollisionIterPropertiesGetNext;
    }

    b2World* CompCollisionObjectGetBox2DWorld(void* _world)
    {
        CollisionWorld* world = (CollisionWorld*)_world;
        if (world->m_3D)
            return 0;
        return (b2World*)dmPhysics::GetWorldContext2D(world->m_World2D);
    }

    b2Body* CompCollisionObjectGetBox2DBody(void* _component)
    {
        CollisionComponent* component = (CollisionComponent*)_component;
        if (component->m_3D)
            return 0;
        return (b2Body*)dmPhysics::GetCollisionObjectContext2D(component->m_Object2D);
    }

    // We use this to determine if a physics object is still alive, by determinig if the game object is still alive
    dmGameObject::HInstance CompCollisionObjectGetInstance(void* _user_data)
    {
        CollisionComponent* component = (CollisionComponent*)_user_data; // See SetCollisionObjectData and dmPhysics::NewCollisionObject2D
        return component->m_Instance;
    }
}
