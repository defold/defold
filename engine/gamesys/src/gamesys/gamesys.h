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

#ifndef DM_GAMESYS_H
#define DM_GAMESYS_H

#include <stdint.h>
#include <string.h>
#include <script/script.h>

#include <resource/resource.h>
#include <dlib/jobsystem.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/lua/lua.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/resources/res_collision_object.h>

#include <gui/gui.h>
#include <input/input.h>
#include <render/render.h>
#include <physics/physics.h>
#include <platform/platform_window.h>

namespace dmMessage { struct URL; }

namespace dmGameSystem
{
    /// Config key to use for tweaking maximum number of collision objects
    extern const char* PHYSICS_MAX_COLLISION_OBJECTS_KEY;
    /// Config key to use for tweaking maximum number of collisions reported
    extern const char* PHYSICS_MAX_COLLISIONS_KEY;
    /// Config key to use for tweaking maximum number of contacts reported
    extern const char* PHYSICS_MAX_CONTACTS_KEY;
    /// Config key to use for tweaking the physics frame rate
    extern const char* PHYSICS_USE_FIXED_TIMESTEP;
    /// Config key for using max updates during a single step
    extern const char* PHYSICS_MAX_FIXED_TIMESTEPS;
    /// Config key for Box2D 2.2 velocity iterations
    extern const char* BOX2D_VELOCITY_ITERATIONS;
    /// Config key for Box2D 2.2 position iterations
    extern const char* BOX2D_POSITION_ITERATIONS;
    /// Config key for Box2D 3.x sub-step count
    extern const char* BOX2D_SUB_STEP_COUNT;
    /// Config key to use for tweaking maximum number of collection proxies
    extern const char* COLLECTION_PROXY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of factories
    extern const char* FACTORY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of collection factories
    extern const char* COLLECTION_FACTORY_MAX_COUNT_KEY;

    struct TilemapContext
    {
        TilemapContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxTilemapCount;
        uint32_t                    m_MaxTileCount;
    };

    struct LabelContext
    {
        LabelContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxLabelCount;
        uint32_t                    m_Subpixels : 1;
    };

    enum PhysicsEngineType
    {
        PHYSICS_ENGINE_NONE,
        PHYSICS_ENGINE_BOX2D,
        PHYSICS_ENGINE_BULLET3D,
    };

    struct CollisionWorld;
    struct CollisionComponent;
    struct ShapeInfo;

    typedef bool              (*IsEnabledFn)(CollisionWorld* world, CollisionComponent* component);
    typedef void              (*WakeupCollisionFn)(CollisionWorld* world, CollisionComponent* component);
    typedef void              (*RayCastFn)(CollisionWorld* world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results);
    typedef void              (*SetGravityFn)(CollisionWorld* world, const dmVMath::Vector3& gravity);
    typedef dmVMath::Vector3  (*GetGravityFn)(CollisionWorld* world);
    typedef void              (*SetCollisionFlipHFn)(CollisionWorld* world, CollisionComponent* component, bool flip);
    typedef void              (*SetCollisionFlipVFn)(CollisionWorld* world, CollisionComponent* component, bool flip);
    typedef dmhash_t          (*GetCollisionGroupFn)(CollisionWorld* world, CollisionComponent* component);
    typedef bool              (*SetCollisionGroupFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash);
    typedef bool              (*GetCollisionMaskBitFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool* maskbit);
    typedef bool              (*SetCollisionMaskBitFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool boolvalue);
    typedef void              (*UpdateMassFn)(CollisionWorld* world, CollisionComponent* component, float mass);
    typedef bool              (*GetShapeIndexFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t shape_name_hash, uint32_t* index_out);
    typedef bool              (*GetShapeFn)(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);
    typedef bool              (*SetShapeFn)(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);
    typedef PhysicsEngineType (*GetPhysicsEngineTypeFn)(CollisionWorld* world);

    typedef dmPhysics::JointResult (*CreateJointFn)(CollisionWorld* world, CollisionComponent* component_a, dmhash_t id, const dmVMath::Point3& apos, CollisionComponent* component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params);
    typedef dmPhysics::JointResult (*DestroyJointFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id);
    typedef dmPhysics::JointResult (*GetJointParamsFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params);
    typedef dmPhysics::JointResult (*GetJointTypeFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type);
    typedef dmPhysics::JointResult (*SetJointParamsFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params);
    typedef dmPhysics::JointResult (*GetJointReactionForceFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmVMath::Vector3& force);
    typedef dmPhysics::JointResult (*GetJointReactionTorqueFn)(CollisionWorld* world, CollisionComponent* component, dmhash_t id, float& torque);

    struct PhysicsAdapterFunctionTable
    {
        IsEnabledFn              m_IsEnabled;
        WakeupCollisionFn        m_WakeupCollision;
        RayCastFn                m_RayCast;
        SetGravityFn             m_SetGravity;
        GetGravityFn             m_GetGravity;
        SetCollisionFlipHFn      m_SetCollisionFlipH;
        SetCollisionFlipVFn      m_SetCollisionFlipV;
        GetCollisionGroupFn      m_GetCollisionGroup;
        SetCollisionGroupFn      m_SetCollisionGroup;
        GetCollisionMaskBitFn    m_GetCollisionMaskBit;
        SetCollisionMaskBitFn    m_SetCollisionMaskBit;
        UpdateMassFn             m_UpdateMass;
        GetShapeIndexFn          m_GetShapeIndex;
        GetShapeFn               m_GetShape;
        SetShapeFn               m_SetShape;
        GetPhysicsEngineTypeFn   m_GetPhysicsEngineType;

        CreateJointFn            m_CreateJoint;
        DestroyJointFn           m_DestroyJoint;
        GetJointParamsFn         m_GetJointParams;
        GetJointTypeFn           m_GetJointType;
        SetJointParamsFn         m_SetJointParams;
        GetJointReactionForceFn  m_GetJointReactionForce;
        GetJointReactionTorqueFn m_GetJointReactionTorque;
    };

    struct PhysicsMessage
    {
        const dmDDF::Descriptor* m_Descriptor; // They're static, so we can store the pointer
        uint32_t m_Offset;  // Offset into payload array
        uint32_t m_Size;    // Size of the data
    };

    struct CollisionWorld
    {
        PhysicsAdapterFunctionTable* m_AdapterFunctions;
        dmScript::LuaCallbackInfo*   m_CallbackInfo;
        uint64_t                     m_Groups[16];
        // We use this array to store messages for later dispatch
        dmArray<uint8_t>             m_MessageData;
        dmArray<PhysicsMessage>      m_MessageInfos;
        uint8_t                      m_CallbackInfoBatched:1;
    };

    struct CollisionComponent
    {
        CollisionObjectResource*      m_Resource;
        dmGameObject::HInstance       m_Instance;
        uint16_t                      m_Mask;
        uint16_t                      m_ComponentIndex;
        // Tracking initial state.
        uint8_t                       m_AddedToUpdate  : 1;
        uint8_t                       m_StartAsEnabled : 1;
        uint8_t                       m_EventMask      : 3;
    };

    struct PhysicsContext
    {
        uint32_t          m_MaxCollisionCount;
        uint32_t          m_MaxCollisionObjectCount;
        uint32_t          m_MaxContactPointCount;
        bool              m_Debug;
        bool              m_UseFixedTimestep;
        uint32_t          m_MaxFixedTimesteps;
        uint32_t          m_Box2DVelocityIterations;
        uint32_t          m_Box2DPositionIterations;
        uint32_t          m_Box2DSubStepCount;
        PhysicsEngineType m_PhysicsType;
    };

    struct PhysicsContextBox2D
    {
        PhysicsContext        m_BaseContext;
        dmPhysics::HContext2D m_Context;
    };

    struct PhysicsContextBullet3D
    {
        PhysicsContext        m_BaseContext;
        dmPhysics::HContext3D m_Context;
    };

    struct ParticleFXContext
    {
        ParticleFXContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        dmRender::HRenderContext m_RenderContext;
        uint32_t m_MaxParticleFXCount;
        uint32_t m_MaxParticleBufferCount;
        uint32_t m_MaxParticleCount;
        uint32_t m_MaxEmitterCount;
        bool m_Debug;
    };

    struct RenderScriptPrototype
    {
        dmArray<void*>                  m_RenderResources;
        dmhash_t                        m_NameHash;
        dmRender::HRenderScriptInstance m_Instance;
        dmRender::HRenderScript         m_Script;
    };

    struct SpriteContext
    {
        SpriteContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxSpriteCount;
        uint32_t                    m_Subpixels : 1;
    };

    struct ModelContext
    {
        ModelContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        dmResource::HFactory        m_Factory;
        uint32_t                    m_MaxModelCount;
        uint16_t                    m_MaxBoneMatrixTextureWidth;
        uint16_t                    m_MaxBoneMatrixTextureHeight;
    };

    struct ScriptLibContext
    {
        ScriptLibContext();

        lua_State*              m_LuaState;
        dmResource::HFactory    m_Factory;
        dmGameObject::HRegister m_Register;
        dmHID::HContext         m_HidContext;
        dmGraphics::HContext    m_GraphicsContext;
        HJobContext             m_JobContext;
        dmScript::HContext      m_ScriptContext;
        dmConfigFile::HConfig   m_ConfigFile;
        dmPlatform::HWindow     m_Window;
    };


    struct CollectionProxyContext
    {
        CollectionProxyContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        uint32_t m_MaxCollectionProxyCount;
    };

    struct FactoryContext
    {
        FactoryContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        dmScript::HContext m_ScriptContext;
        uint32_t m_MaxFactoryCount;
    };

    struct CollectionFactoryContext
    {
        CollectionFactoryContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        dmScript::HContext m_ScriptContext;
        uint32_t m_MaxCollectionFactoryCount;
    };

    bool InitializeScriptLibs(const ScriptLibContext& context);
    void FinalizeScriptLibs(const ScriptLibContext& context);
    void UpdateScriptLibs(const ScriptLibContext& context);

    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory,
        dmRender::HRenderContext render_context,
        dmInput::HContext input_context,
        PhysicsContext* physics_context);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  ParticleFXContext* emitter_context,
                                                  SpriteContext* sprite_context,
                                                  CollectionProxyContext* collection_proxy_context,
                                                  FactoryContext* factory_context,
                                                  CollectionFactoryContext *collectionfactory_context,
                                                  ModelContext* model_context,
                                                  LabelContext* label_context,
                                                  TilemapContext* tilemap_context);

    void OnWindowFocus(bool focus);
    void OnWindowIconify(bool iconfiy);
    void OnWindowResized(int width, int height);
    void OnWindowCreated(int width, int height);
}

#endif // DM_GAMESYS_H
