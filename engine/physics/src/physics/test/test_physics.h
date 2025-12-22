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

#ifndef PHYSICS_TEST_PHYSICS_H
#define PHYSICS_TEST_PHYSICS_H

#include <stdint.h>
#include "../physics.h"
#include <jc_test/jc_test.h>

template <typename T> class dmArray;

struct VisualObject
{
    VisualObject();

    dmVMath::Point3         m_Position;
    dmVMath::Quat           m_Rotation;
    float                   m_Scale;
    int                     m_CollisionCount;
    uint16_t                m_FirstCollisionGroup;
};

void GetWorldTransform(void* visual_object, dmTransform::Transform& world_transform);
void SetWorldTransform(void* visual_object, const dmVMath::Point3& position, const dmVMath::Quat& rotation);
bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data);
bool ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data);

static const float PHYSICS_SCALE = 0.5f;
static const float PRECISION_ROUNDING_PHYSICS_SCALE = 0.00999999978f;

template<typename T>
class PhysicsTest : public jc_test_base_class
{
protected:

    void SetUp() override
    {
        SetupContextAndWorld(PHYSICS_SCALE);
    }

    void SetupContextAndWorld(float physics_scale)
    {
        dmPhysics::NewContextParams context_params = dmPhysics::NewContextParams();
        context_params.m_Scale = physics_scale;
        context_params.m_RayCastLimit2D = 64;
        context_params.m_RayCastLimit3D = 128;
        context_params.m_TriggerOverlapCapacity = 16;
        m_Context = (*m_Test.m_NewContextFunc)(context_params);
        dmPhysics::NewWorldParams world_params;
        world_params.m_GetWorldTransformCallback = GetWorldTransform;
        world_params.m_SetWorldTransformCallback = SetWorldTransform;
        world_params.m_MaxCollisionObjectsCount = 1024;
        m_World = (*m_Test.m_NewWorldFunc)(m_Context, world_params);
        m_CollisionCount = 0;
        m_ContactPointCount = 0;
        m_StepWorldContext.m_DT = 1.0f / 60.0f;
        m_StepWorldContext.m_CollisionCallback = CollisionCallback;
        m_StepWorldContext.m_CollisionUserData = &m_CollisionCount;
        m_StepWorldContext.m_ContactPointCallback = ContactPointCallback;
        m_StepWorldContext.m_ContactPointUserData = &m_ContactPointCount;
        m_StepWorldContext.m_MaxFixedTimeSteps = 2;
        m_StepWorldContext.m_Box2DVelocityIterations = 10;
        m_StepWorldContext.m_Box2DPositionIterations = 10;
        m_StepWorldContext.m_Box2DSubStepCount = 10;
    }

    void TearDown() override
    {
        (*m_Test.m_DeleteWorldFunc)(m_Context, m_World);
        (*m_Test.m_DeleteContextFunc)(m_Context);
    }

    typename T::ContextType m_Context;
    typename T::WorldType m_World;
    T m_Test;
    dmPhysics::StepWorldContext m_StepWorldContext;
    int m_CollisionCount;
    int m_ContactPointCount;
};

template<typename T>
class PhysicsPrecisionRoundingTest : public PhysicsTest<T>
{
protected:

    virtual void SetUp()
    {
        this->SetupContextAndWorld(PRECISION_ROUNDING_PHYSICS_SCALE);
    }
};

template<typename T>
struct Funcs
{
    typedef typename T::ContextType (*NewContextFunc)(const dmPhysics::NewContextParams& params);
    typedef void (*DeleteContextFunc)(typename T::ContextType context);
    typedef typename T::WorldType (*NewWorldFunc)(typename T::ContextType context, const dmPhysics::NewWorldParams& params);
    typedef void (*DeleteWorldFunc)(typename T::ContextType context, typename T::WorldType world);
    typedef void (*StepWorldFunc)(typename T::WorldType world, const dmPhysics::StepWorldContext& context);
    typedef void (*SetCollisionCallbackFunc)(typename T::WorldType world, dmPhysics::CollisionCallback callback, void* user_data);
    typedef void (*SetContactPointCallbackFunc)(typename T::WorldType world, dmPhysics::ContactPointCallback callback, void* user_data);
    typedef void (*SetDrawDebugFunc)(typename T::WorldType world, bool);
    typedef typename T::CollisionShapeType (*NewBoxShapeFunc)(typename T::ContextType context, const dmVMath::Vector3& half_extents);
    typedef typename T::CollisionShapeType (*NewSphereShapeFunc)(typename T::ContextType context, float radius);
    typedef typename T::CollisionShapeType (*NewCapsuleShapeFunc)(typename T::ContextType context, float radius, float height);
    typedef typename T::CollisionShapeType (*NewConvexHullShapeFunc)(typename T::ContextType context, const float* vertices, uint32_t vertex_count);
    typedef void (*DeleteCollisionShapeFunc)(typename T::CollisionShapeType shape);
    typedef typename T::CollisionObjectType (*NewCollisionObjectFunc)(typename T::WorldType world, const dmPhysics::CollisionObjectData& data, typename T::CollisionShapeType* shapes, uint32_t shape_count);
    typedef typename T::CollisionObjectType (*NewCollisionObjectFunc2)(typename T::WorldType world, const dmPhysics::CollisionObjectData& data, typename T::CollisionShapeType* shapes, dmVMath::Vector3* translations, dmVMath::Quat* rotations, uint32_t shape_count);
    typedef void (*DeleteCollisionObjectFunc)(typename T::WorldType world, typename T::CollisionObjectType collision_object);
    typedef uint32_t (*GetCollisionShapesFunc)(typename T::CollisionObjectType collision_object, typename T::CollisionShapeType* out_buffer, uint32_t buffer_size);
    typedef void (*SetCollisionObjectUserDataFunc)(typename T::CollisionObjectType collision_object, void* user_data);
    typedef void* (*GetCollisionObjectUserDataFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*ApplyForceFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object, const dmVMath::Vector3& force, const dmVMath::Point3& position);
    typedef dmVMath::Vector3 (*GetTotalForceFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object);
    typedef dmVMath::Point3 (*GetWorldPositionFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object);
    typedef dmVMath::Quat (*GetWorldRotationFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object);
    typedef dmVMath::Vector3 (*GetLinearVelocityFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object);
    typedef dmVMath::Vector3 (*GetAngularVelocityFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object);
    typedef void (*SetLinearVelocityFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object, const dmVMath::Vector3& velocity);
    typedef void (*SetAngularVelocityFunc)(typename T::ContextType context, typename T::CollisionObjectType collision_object, const dmVMath::Vector3& velocity);
    typedef bool (*IsEnabledFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*SetEnabledFunc)(typename T::WorldType world, typename T::CollisionObjectType collision_object, bool enabled);
    typedef bool (*IsSleepingFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*WakeupFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*SetLockedRotationFunc)(typename T::CollisionObjectType collision_object, bool locked_rotation);
    typedef float (*GetLinearDampingFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*SetLinearDampingFunc)(typename T::CollisionObjectType collision_object, float linear_damping);
    typedef float (*GetAngularDampingFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*SetAngularDampingFunc)(typename T::CollisionObjectType collision_object, float angular_damping);
    typedef float (*GetMassFunc)(typename T::CollisionObjectType collision_object);
    typedef bool (*RequestRayCastFunc)(typename T::WorldType world, const dmPhysics::RayCastRequest& request);
    typedef void (*RayCastFunc)(typename T::WorldType world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results);
    typedef void (*SetDebugCallbacks)(typename T::ContextType context, const dmPhysics::DebugCallbacks& callbacks);
    typedef void (*ReplaceShapeFunc)(typename T::ContextType context, typename T::CollisionShapeType old_shape, typename T::CollisionShapeType new_shape);
    typedef void (*SetGravityFunc)(typename T::WorldType world, const dmVMath::Vector3& gravity);
    typedef bool (*IsBulletFunc)(typename T::CollisionObjectType collision_object);
    typedef void (*SetBulletFunc)(typename T::CollisionObjectType collision_object, bool value);
    typedef void* (*GetWorldContextFunc)(typename T::WorldType world);
    typedef void* (*GetCollisionObjectContextFunc)(typename T::CollisionObjectType collision_object);
    typedef dmVMath::Vector3 (*GetGravityFunc)(typename T::WorldType world);
};

struct Test3D
{
    Test3D();
    ~Test3D();

    typedef dmPhysics::HContext3D ContextType;
    typedef dmPhysics::HWorld3D WorldType;
    typedef dmPhysics::HCollisionObject3D CollisionObjectType;
    typedef dmPhysics::HCollisionShape3D CollisionShapeType;

    Funcs<Test3D>::NewContextFunc                   m_NewContextFunc;
    Funcs<Test3D>::DeleteContextFunc                m_DeleteContextFunc;
    Funcs<Test3D>::NewWorldFunc                     m_NewWorldFunc;
    Funcs<Test3D>::DeleteWorldFunc                  m_DeleteWorldFunc;
    Funcs<Test3D>::StepWorldFunc                    m_StepWorldFunc;
    Funcs<Test3D>::SetCollisionCallbackFunc         m_SetCollisionCallbackFunc;
    Funcs<Test3D>::SetContactPointCallbackFunc      m_SetContactPointCallbackFunc;
    Funcs<Test3D>::SetDrawDebugFunc                 m_SetDrawDebugFunc;
    Funcs<Test3D>::NewBoxShapeFunc                  m_NewBoxShapeFunc;
    Funcs<Test3D>::NewSphereShapeFunc               m_NewSphereShapeFunc;
    Funcs<Test3D>::NewCapsuleShapeFunc              m_NewCapsuleShapeFunc;
    Funcs<Test3D>::NewConvexHullShapeFunc           m_NewConvexHullShapeFunc;
    Funcs<Test3D>::DeleteCollisionShapeFunc         m_DeleteCollisionShapeFunc;
    Funcs<Test3D>::NewCollisionObjectFunc           m_NewCollisionObjectFunc;
    Funcs<Test3D>::NewCollisionObjectFunc2          m_NewCollisionObjectFunc2;
    Funcs<Test3D>::DeleteCollisionObjectFunc        m_DeleteCollisionObjectFunc;
    Funcs<Test3D>::GetCollisionShapesFunc           m_GetCollisionShapesFunc;
    Funcs<Test3D>::SetCollisionObjectUserDataFunc   m_SetCollisionObjectUserDataFunc;
    Funcs<Test3D>::GetCollisionObjectUserDataFunc   m_GetCollisionObjectUserDataFunc;
    Funcs<Test3D>::ApplyForceFunc                   m_ApplyForceFunc;
    Funcs<Test3D>::GetTotalForceFunc                m_GetTotalForceFunc;
    Funcs<Test3D>::GetWorldPositionFunc             m_GetWorldPositionFunc;
    Funcs<Test3D>::GetWorldRotationFunc             m_GetWorldRotationFunc;
    Funcs<Test3D>::GetLinearVelocityFunc            m_GetLinearVelocityFunc;
    Funcs<Test3D>::GetAngularVelocityFunc           m_GetAngularVelocityFunc;
    Funcs<Test3D>::SetLinearVelocityFunc            m_SetLinearVelocityFunc;
    Funcs<Test3D>::SetAngularVelocityFunc           m_SetAngularVelocityFunc;
    Funcs<Test3D>::IsEnabledFunc                    m_IsEnabledFunc;
    Funcs<Test3D>::SetEnabledFunc                   m_SetEnabledFunc;
    Funcs<Test3D>::IsSleepingFunc                   m_IsSleepingFunc;
    Funcs<Test3D>::WakeupFunc                       m_WakeupFunc;
    Funcs<Test3D>::SetLockedRotationFunc            m_SetLockedRotationFunc;
    Funcs<Test3D>::GetLinearDampingFunc             m_GetLinearDampingFunc;
    Funcs<Test3D>::SetLinearDampingFunc             m_SetLinearDampingFunc;
    Funcs<Test3D>::GetAngularDampingFunc            m_GetAngularDampingFunc;
    Funcs<Test3D>::SetAngularDampingFunc            m_SetAngularDampingFunc;
    Funcs<Test3D>::GetMassFunc                      m_GetMassFunc;
    Funcs<Test3D>::RequestRayCastFunc               m_RequestRayCastFunc;
    Funcs<Test3D>::RayCastFunc                      m_RayCastFunc;
    Funcs<Test3D>::SetDebugCallbacks                m_SetDebugCallbacksFunc;
    Funcs<Test3D>::ReplaceShapeFunc                 m_ReplaceShapeFunc;
    Funcs<Test3D>::SetGravityFunc                   m_SetGravityFunc;
    Funcs<Test3D>::GetGravityFunc                   m_GetGravityFunc;

    float*      m_Vertices;
    uint32_t    m_VertexCount;
    float       m_PolygonRadius;
    bool        m_Is3D;
};

struct Test2D
{
    Test2D();
    ~Test2D();

    typedef dmPhysics::HContext2D ContextType;
    typedef dmPhysics::HWorld2D WorldType;
    typedef dmPhysics::HCollisionObject2D CollisionObjectType;
    typedef dmPhysics::HCollisionShape2D CollisionShapeType;

    Funcs<Test2D>::NewContextFunc                   m_NewContextFunc;
    Funcs<Test2D>::DeleteContextFunc                m_DeleteContextFunc;
    Funcs<Test2D>::NewWorldFunc                     m_NewWorldFunc;
    Funcs<Test2D>::DeleteWorldFunc                  m_DeleteWorldFunc;
    Funcs<Test2D>::StepWorldFunc                    m_StepWorldFunc;
    Funcs<Test2D>::SetDrawDebugFunc                 m_SetDrawDebugFunc;
    Funcs<Test2D>::NewBoxShapeFunc                  m_NewBoxShapeFunc;
    Funcs<Test2D>::NewSphereShapeFunc               m_NewSphereShapeFunc;
    Funcs<Test2D>::NewCapsuleShapeFunc              m_NewCapsuleShapeFunc;
    Funcs<Test2D>::NewConvexHullShapeFunc           m_NewConvexHullShapeFunc;
    Funcs<Test2D>::DeleteCollisionShapeFunc         m_DeleteCollisionShapeFunc;
    Funcs<Test2D>::NewCollisionObjectFunc           m_NewCollisionObjectFunc;
    Funcs<Test2D>::NewCollisionObjectFunc2          m_NewCollisionObjectFunc2;
    Funcs<Test2D>::DeleteCollisionObjectFunc        m_DeleteCollisionObjectFunc;
    Funcs<Test2D>::GetCollisionShapesFunc           m_GetCollisionShapesFunc;
    Funcs<Test2D>::SetCollisionObjectUserDataFunc   m_SetCollisionObjectUserDataFunc;
    Funcs<Test2D>::GetCollisionObjectUserDataFunc   m_GetCollisionObjectUserDataFunc;
    Funcs<Test2D>::ApplyForceFunc                   m_ApplyForceFunc;
    Funcs<Test2D>::GetTotalForceFunc                m_GetTotalForceFunc;
    Funcs<Test2D>::GetWorldPositionFunc             m_GetWorldPositionFunc;
    Funcs<Test2D>::GetWorldRotationFunc             m_GetWorldRotationFunc;
    Funcs<Test2D>::GetLinearVelocityFunc            m_GetLinearVelocityFunc;
    Funcs<Test2D>::GetAngularVelocityFunc           m_GetAngularVelocityFunc;
    Funcs<Test2D>::SetLinearVelocityFunc            m_SetLinearVelocityFunc;
    Funcs<Test2D>::SetAngularVelocityFunc           m_SetAngularVelocityFunc;
    Funcs<Test2D>::IsEnabledFunc                    m_IsEnabledFunc;
    Funcs<Test2D>::SetEnabledFunc                   m_SetEnabledFunc;
    Funcs<Test2D>::IsSleepingFunc                   m_IsSleepingFunc;
    Funcs<Test2D>::WakeupFunc                       m_WakeupFunc;
    Funcs<Test2D>::SetLockedRotationFunc            m_SetLockedRotationFunc;
    Funcs<Test2D>::GetLinearDampingFunc             m_GetLinearDampingFunc;
    Funcs<Test2D>::SetLinearDampingFunc             m_SetLinearDampingFunc;
    Funcs<Test2D>::GetAngularDampingFunc            m_GetAngularDampingFunc;
    Funcs<Test2D>::SetAngularDampingFunc            m_SetAngularDampingFunc;
    Funcs<Test2D>::GetMassFunc                      m_GetMassFunc;
    Funcs<Test2D>::RequestRayCastFunc               m_RequestRayCastFunc;
    Funcs<Test2D>::RayCastFunc                      m_RayCastFunc;
    Funcs<Test2D>::SetDebugCallbacks                m_SetDebugCallbacksFunc;
    Funcs<Test2D>::ReplaceShapeFunc                 m_ReplaceShapeFunc;
    Funcs<Test2D>::SetGravityFunc                   m_SetGravityFunc;
    Funcs<Test2D>::GetGravityFunc                   m_GetGravityFunc;
    Funcs<Test2D>::IsBulletFunc                     m_IsBulletFunc;
    Funcs<Test2D>::SetBulletFunc                    m_SetBulletFunc;
    Funcs<Test2D>::GetWorldContextFunc              m_GetWorldContextFunc;
    Funcs<Test2D>::GetCollisionObjectContextFunc    m_GetCollisionObjectContextFunc;

    float*      m_Vertices;
    uint32_t    m_VertexCount;
    float       m_PolygonRadius;
    bool        m_Is3D;
};

#endif // PHYSICS_TEST_PHYSICS_H
