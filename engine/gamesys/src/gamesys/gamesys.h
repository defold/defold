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

#include <input/input.h>
#include <render/render.h>
#include <physics/physics.h>
#include <platform/window.h>

namespace dmMessage { struct URL; }

namespace dmGameSystem
{
    struct Context;
    typedef Context* HContext;
    struct ComponentRenderType;

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
        uint32_t                    m_ComponentTypeIndex;
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

    enum PhysicsMessageType
    {
        PHYSICS_MESSAGE_TYPE_COLLISION,
        PHYSICS_MESSAGE_TYPE_CONTACT_POINT,
        PHYSICS_MESSAGE_TYPE_TRIGGER,
        PHYSICS_MESSAGE_TYPE_RAY_CAST_RESPONSE,
        PHYSICS_MESSAGE_TYPE_RAY_CAST_MISSED,
    };

    struct PhysicsMessage
    {
        uint32_t           m_Offset; // Aligned offset into payload array
        PhysicsMessageType m_Type;
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
        dmResource::HFactory        m_Factory;
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
        HWindow                 m_Window;
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

    /**
     * Borrowed services for a game-system instance. The context owns its built-in
     * component contexts and render bridge, not these services. Internal, not an SDK API.
     */
    struct ContextParams
    {
        dmResource::HFactory     m_Factory;
        dmGameObject::HRegister  m_Register;
        dmRender::HRenderContext m_RenderContext;
        dmScript::HContext       m_ScriptContext;
        dmInput::HContext        m_InputContext;
        PhysicsContext*          m_PhysicsContext;
    };

    /**
     * Create game-system state on the game owner, before registering component types.
     * Creates no production thread; frame storage is allocated only when a type opts in.
     * @param params Borrowed services, kept alive through Finalize
     * @return New context, or null on render-bridge allocation failure
     */
    HContext NewContext(const ContextParams& params);

    /**
     * Configure built-in component contexts and register their resources/components once.
     * Call after resource descriptors are registered, before component descriptors or worlds.
     * A failed initialization still requires Finalize and DeleteContext; the resource
     * factory and register retain any types already registered for normal shutdown.
     * @param context Game-system instance
     * @param config Project configuration, read only during initialization
     * @return True on success, false on registration failure
     */
    bool Initialize(HContext context, dmConfigFile::HConfig config);

    /**
     * Register a component and optional render-input callbacks together, before worlds exist.
     * Failure leaves neither a new game type nor a render binding registered.
     * @param context Game-system instance
     * @param type Game-side component registration
     * @param render_type Optional owned-input capture and renderer callbacks; copied on success
     * @return Game-object registration result
     */
    dmGameObject::Result RegisterComponentType(HContext context, const dmGameObject::ComponentType& type, const ComponentRenderType* render_type = 0);

    /**
     * Begin a game-system frame before jobs, resource reloads, scripts and simulation.
     * Handles outstanding work internally so simulation cannot run ahead of rendering.
     * Finish with PostUpdate even when drawing is skipped, or Finalize when exiting.
     * @param context Game-system instance
     * @return True if the frame can begin; errors are logged internally
     */
    bool BeginFrame(HContext context);

    /**
     * Render components after final transforms/PreRender, before destructive PostUpdate.
     * Requires successful BeginFrame. Rendering and handoff use the selected execution
     * mode internally; callers use the same frame phases in either mode. Capture failures
     * are logged and cancel that frame; persistent renderer errors also fail PostUpdate.
     * @param context Game-system instance
     * @param collection Root collection to render
     * @return True on success, false on capture or consumer failure
     */
    bool Render(HContext context, dmGameObject::HCollection collection);

    /**
     * Finish the frame after drawing, including frames where drawing was skipped. Performs
     * collection and register PostUpdate, with internal frame retirement before game-world
     * cleanup and deferred renderer cleanup afterwards. Game-object callback errors retain
     * their existing non-fatal policy; service or persistent renderer errors return false.
     * @param context Game-system instance
     * @param collection Root collection, or null when only register cleanup is needed
     * @return True if frame processing can continue; errors are logged internally
     */
    bool PostUpdate(HContext context, dmGameObject::HCollection collection);

    /**
     * Finish pending work and release internal rendering state after all collections have
     * been deleted. Coordinates executor shutdown internally before deleting callback state.
     * Keeps built-in context storage alive for resource destruction. Also used after failed
     * initialization or an unfinished frame. No frame work is allowed afterwards.
     * @param context Game-system instance on the game owner
     * @return True when finalized; false leaves state alive and logs the shutdown error
     */
    bool Finalize(HContext context);

    /**
     * Free context storage after Finalize and destruction of the resource factory/register.
     * This does not access or destroy borrowed services.
     * @param context Finalized game-system instance
     */
    void DeleteContext(HContext context);

    bool InitializeScriptLibs(const ScriptLibContext& context);
    void FinalizeScriptLibs(const ScriptLibContext& context);
    void UpdateScriptLibs(const ScriptLibContext& context);

    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory,
        dmRender::HRenderContext render_context,
        dmInput::HContext input_context,
        PhysicsContext* physics_context,
        ModelContext* model_context);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  SpriteContext* sprite_context,
                                                  CollectionProxyContext* collection_proxy_context,
                                                  FactoryContext* factory_context,
                                                  CollectionFactoryContext *collectionfactory_context,
                                                  ModelContext* model_context,
                                                  LabelContext* label_context);

    void OnWindowFocus(bool focus);
    void OnWindowIconify(bool iconfiy);
    void OnWindowResized(int width, int height);
    void OnWindowCreated(int width, int height);
}

#endif // DM_GAMESYS_H
