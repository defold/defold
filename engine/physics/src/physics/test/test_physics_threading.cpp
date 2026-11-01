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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <box2d/constants.h>
#include <dlib/dstrings.h>
#include <dlib/thread.h>
#include <dlib/atomic.h>
#include <dlib/condition_variable.h>
#include <dlib/mutex.h>

#include "../box2d/box2d_physics.h"

static void CreateContext(int workers, dmPhysics::HContext2D* context)
{
    char setting[64];
    dmSnPrintf(setting, sizeof(setting), "--config=box2d.worker_count=%d", workers);
    const char* argv[] = { "test_physics_threading", setting };
    dmConfigFile::HConfig config = 0;
    ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer(0, 0, 2, argv, &config));
    dmPhysics::NewContextParams params;
    params.m_Config = config;
    *context = dmPhysics::NewContext2D(params);
    dmConfigFile::Delete(config);
    ASSERT_NE((void*)0, *context);
}

// Default/off, on and clamped settings must reach Box2D, including the serial platform fallback.
TEST(Box2DThreading, WorkerConfiguration)
{
    const int settings[] = { 0, 1, 3, -1, 2147483647 };
    const int expected[] = { 1, 2, 4, 1, B2_MAX_WORKERS };
    bool has_thread_support = false;
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    has_thread_support = dmThread::PlatformHasThreadSupport();
#endif
    for (uint32_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
    {
        dmPhysics::HContext2D context = 0;
        CreateContext(settings[i], &context);
        dmPhysics::NewWorldParams params;
        dmPhysics::HWorld2D world = dmPhysics::NewWorld2D(context, params);
        ASSERT_NE((void*)0, world);
        ASSERT_EQ(has_thread_support ? expected[i] : 1, b2World_GetWorkerCount(world->m_WorldId));
        ASSERT_EQ(has_thread_support && settings[i] > 0, world->m_TaskContext != 0);
        dmPhysics::DeleteWorld2D(context, world);
        dmPhysics::DeleteContext2D(context);
    }

    dmPhysics::NewContextParams params;
    dmPhysics::HContext2D context = dmPhysics::NewContext2D(params);
    dmPhysics::HWorld2D world = dmPhysics::NewWorld2D(context, dmPhysics::NewWorldParams());
    ASSERT_EQ(1, b2World_GetWorkerCount(world->m_WorldId));
    dmPhysics::DeleteWorld2D(context, world);
    dmPhysics::DeleteContext2D(context);
}

struct Callbacks
{
    dmThread::TlsKey m_Key;
    uint32_t         m_Collisions;
    uint32_t         m_Contacts;
    bool             m_OnCallingThread;
    uint64_t         m_Hash;
    bool             m_StopCallbacks;
    bool             m_MutateCallbacks;
};

struct Body
{
    b2BodyId        m_PhysicsBody;
    uint32_t        m_Id;
    dmVMath::Point3 m_Position;
    dmVMath::Quat   m_Rotation;
    Callbacks*      m_Callbacks;
};

static void CheckCallingThread(Callbacks* callbacks)
{
    callbacks->m_OnCallingThread &= dmThread::GetTlsValue(callbacks->m_Key) == callbacks;
}

static void GetTransform(void* user_data, dmTransform::Transform& transform)
{
    Body* body = (Body*)user_data;
    CheckCallingThread(body->m_Callbacks);
    transform.SetIdentity();
    transform.SetTranslation(dmVMath::Vector3(body->m_Position));
    transform.SetRotation(body->m_Rotation);
}

static void SetTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation)
{
    Body* body = (Body*)user_data;
    CheckCallingThread(body->m_Callbacks);
    body->m_Position = position;
    body->m_Rotation = rotation;
}

static void HashBytes(Callbacks* callbacks, const void* data, uint32_t size)
{
    const uint8_t* bytes = (const uint8_t*)data;
    for (uint32_t i = 0; i < size; ++i)
    {
        callbacks->m_Hash = (callbacks->m_Hash ^ bytes[i]) * 1099511628211ULL;
    }
}

static bool Collision(void* a, uint16_t group_a, void* b, uint16_t group_b, void* user_data)
{
    Callbacks* callbacks = (Callbacks*)user_data;
    CheckCallingThread(callbacks);
    ++callbacks->m_Collisions;
    // Probe legacy immediate callbacks, which may change data consumed by later contacts.
    if (callbacks->m_MutateCallbacks)
    {
        const b2Vec2 velocity = { 10.0f, 0.0f };
        b2Body_SetLinearVelocity(((Body*)b)->m_PhysicsBody, velocity);
    }
    const uint32_t values[] = { ((Body*)a)->m_Id, group_a, ((Body*)b)->m_Id, group_b };
    HashBytes(callbacks, values, sizeof(values));
    return !callbacks->m_StopCallbacks || callbacks->m_Collisions < 3;
}

static bool Contact(const dmPhysics::ContactPoint& point, void* user_data)
{
    Callbacks* callbacks = (Callbacks*)user_data;
    CheckCallingThread(callbacks);
    ++callbacks->m_Contacts;
    const uint32_t ids[] = { ((Body*)point.m_UserDataA)->m_Id, point.m_GroupA, ((Body*)point.m_UserDataB)->m_Id, point.m_GroupB };
    HashBytes(callbacks, ids, sizeof(ids));
    const float values[] = { point.m_PositionA.getX(), point.m_PositionA.getY(), point.m_PositionA.getZ(), point.m_PositionB.getX(), point.m_PositionB.getY(), point.m_PositionB.getZ(), point.m_Normal.getX(), point.m_Normal.getY(), point.m_Normal.getZ(), point.m_RelativeVelocity.getX(), point.m_RelativeVelocity.getY(), point.m_RelativeVelocity.getZ(), point.m_Distance, point.m_AppliedImpulse, point.m_MassA, point.m_MassB };
    HashBytes(callbacks, values, sizeof(values));
    return !callbacks->m_StopCallbacks || callbacks->m_Contacts < 3;
}

static void CompareThreadedCallbacks(bool read_only, bool stop_callbacks, bool mutate_callbacks)
{
    const uint32_t body_count = 257;
    dmThread::TlsKey key = dmThread::AllocTls();
    const int rates[] = { 30, 60 };
    for (uint32_t rate = 0; rate < 2; ++rate)
    {
        dmPhysics::HContext2D contexts[2];
        dmPhysics::HWorld2D worlds[2];
        dmPhysics::HCollisionShape2D shapes[2][3];
        dmPhysics::HCollisionObject2D objects[2][body_count];
        Body bodies[2][body_count];
        Callbacks callbacks[2];
        for (uint32_t mode = 0; mode < 2; ++mode)
        {
            callbacks[mode].m_Hash = 14695981039346656037ULL;
            callbacks[mode].m_StopCallbacks = stop_callbacks;
            callbacks[mode].m_MutateCallbacks = mutate_callbacks;
            callbacks[mode].m_Key = key;
            callbacks[mode].m_Collisions = 0;
            callbacks[mode].m_Contacts = 0;
            callbacks[mode].m_OnCallingThread = true;
            dmThread::SetTlsValue(key, &callbacks[mode]);
            CreateContext(mode ? 3 : 0, &contexts[mode]);
            dmPhysics::NewWorldParams params;
            params.m_GetWorldTransformCallback = GetTransform;
            params.m_SetWorldTransformCallback = SetTransform;
            params.m_MaxCollisionObjectsCount = body_count;
            worlds[mode] = dmPhysics::NewWorld2D(contexts[mode], params);
            b2World_EnableSleeping(worlds[mode]->m_WorldId, false);
            shapes[mode][0] = dmPhysics::NewBoxShape2D(contexts[mode], dmVMath::Vector3(32.0f, 0.5f, 0.0f));
            shapes[mode][1] = dmPhysics::NewBoxShape2D(contexts[mode], dmVMath::Vector3(0.45f, 0.45f, 0.0f));
            shapes[mode][2] = dmPhysics::NewCircleShape2D(contexts[mode], 0.45f);
            for (uint32_t i = 0; i < body_count; ++i)
            {
                Body* body = &bodies[mode][i];
                body->m_Id = i;
                body->m_Position = i ? dmVMath::Point3(float((i - 1) % 16) - 7.5f, float((i - 1) / 16), 0.0f) : dmVMath::Point3(0.0f, -1.0f, 0.0f);
                body->m_Rotation = dmVMath::Quat::identity();
                body->m_Callbacks = &callbacks[mode];
                dmPhysics::CollisionObjectData data;
                data.m_Type = i ? dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC : dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
                data.m_Mass = i ? 1.0f : 0.0f;
                data.m_UserData = body;
                objects[mode][i] = dmPhysics::NewCollisionObject2D(worlds[mode], data, &shapes[mode][i ? 1 + i % 2 : 0], 1);
                body->m_PhysicsBody = ((dmPhysics::Body*)objects[mode][i])->m_BodyId;
            }
        }

        for (int tick = 0; tick < 2 * rates[rate]; ++tick)
        {
            for (uint32_t mode = 0; mode < 2; ++mode)
            {
                dmThread::SetTlsValue(key, &callbacks[mode]);
                dmPhysics::StepWorldContext step;
                step.m_ContactCallbacksReadOnly = read_only;
                step.m_DT = 1.0f / rates[rate];
                step.m_Box2DSubStepCount = rates[rate] == 30 ? 8 : 4;
                step.m_CollisionCallback = Collision;
                step.m_CollisionUserData = &callbacks[mode];
                step.m_ContactPointCallback = Contact;
                step.m_ContactPointUserData = &callbacks[mode];
                dmPhysics::StepWorld2D(worlds[mode], step);
                ASSERT_TRUE(callbacks[mode].m_OnCallingThread);
            }
            ASSERT_EQ(callbacks[0].m_Collisions, callbacks[1].m_Collisions);
            ASSERT_EQ(callbacks[0].m_Contacts, callbacks[1].m_Contacts);
            ASSERT_EQ(callbacks[0].m_Hash, callbacks[1].m_Hash);
            for (uint32_t i = 1; i < body_count; ++i)
            {
                for (uint32_t axis = 0; axis < 3; ++axis)
                {
                    ASSERT_EQ(bodies[0][i].m_Position[axis], bodies[1][i].m_Position[axis]);
                }
                ASSERT_EQ(bodies[0][i].m_Rotation.getZ(), bodies[1][i].m_Rotation.getZ());
                ASSERT_EQ(bodies[0][i].m_Rotation.getW(), bodies[1][i].m_Rotation.getW());
            }
        }
        ASSERT_GT(callbacks[0].m_Contacts, 0u);
        ASSERT_EQ(read_only && worlds[1]->m_TaskContext != 0, worlds[1]->m_ContactCollection != 0);

        dmArray<dmPhysics::RayCastResponse> results[2];
        dmPhysics::RayCastRequest ray;
        ray.m_From = dmVMath::Point3(0.0f, 30.0f, 0.0f);
        ray.m_To = dmVMath::Point3(0.0f, -2.0f, 0.0f);
        ray.m_Mask = 1;
        for (uint32_t mode = 0; mode < 2; ++mode)
        {
            dmPhysics::RayCast2D(worlds[mode], ray, results[mode]);
        }
        ASSERT_EQ(1u, results[0].Size());
        ASSERT_EQ(results[0].Size(), results[1].Size());
        ASSERT_EQ(results[0][0].m_Fraction, results[1][0].m_Fraction);

        for (uint32_t mode = 0; mode < 2; ++mode)
        {
            for (uint32_t i = 0; i < body_count; ++i)
            {
                dmPhysics::DeleteCollisionObject2D(worlds[mode], objects[mode][i]);
            }
            for (uint32_t i = 0; i < 3; ++i)
            {
                dmPhysics::DeleteCollisionShape2D(shapes[mode][i]);
            }
            dmPhysics::DeleteWorld2D(contexts[mode], worlds[mode]);
            dmPhysics::DeleteContext2D(contexts[mode]);
        }
    }
    dmThread::FreeTls(key);
}

// Worker steps preserve transforms, ordered contact data and synchronous queries at both fixed rates.
TEST(Box2DThreading, SerialAndThreadedStateAndCallbacks)
{
    CompareThreadedCallbacks(false, false, false);
}

// Deferred contact collection must deliver identical ordered payloads on the calling thread.
TEST(Box2DThreading, DeferredContactCallbacks)
{
    CompareThreadedCallbacks(true, false, false);
}

// Stopping either callback must discard later collected entries without changing the delivered order.
TEST(Box2DThreading, DeferredCallbacksStopEarly)
{
    CompareThreadedCallbacks(true, true, false);
}

// Stopping solid-contact collisions must also stop sensor collisions later in the same step.
TEST(Box2DThreading, CollisionCancellationIncludesSensors)
{
    dmThread::TlsKey key = dmThread::AllocTls();
    for (uint32_t workers = 0; workers <= 3; workers += 3)
    {
        for (uint32_t read_only = 0; read_only < 2; ++read_only)
        {
            Callbacks callbacks = {};
            callbacks.m_Key = key;
            callbacks.m_OnCallingThread = true;
            dmThread::SetTlsValue(key, &callbacks);
            dmPhysics::HContext2D context = 0;
            CreateContext(workers, &context);
            context->m_TriggerOverlapCapacity = 16;
            dmPhysics::NewWorldParams params;
            params.m_GetWorldTransformCallback = GetTransform;
            dmPhysics::HWorld2D world = dmPhysics::NewWorld2D(context, params);
            dmPhysics::HCollisionShape2D shapes[] = {
                dmPhysics::NewBoxShape2D(context, dmVMath::Vector3(8.0f, 0.5f, 0.0f)),
                dmPhysics::NewBoxShape2D(context, dmVMath::Vector3(0.5f, 0.5f, 0.0f)),
                dmPhysics::NewBoxShape2D(context, dmVMath::Vector3(8.0f, 2.0f, 0.0f)),
            };
            Body bodies[5];
            dmPhysics::HCollisionObject2D objects[5];
            for (uint32_t i = 0; i < 5; ++i)
            {
                Body* body = &bodies[i];
                body->m_Id = i;
                body->m_Position = i == 0 ? dmVMath::Point3(0.0f, -0.5f, 0.0f) : dmVMath::Point3(2.0f * i - 4.0f, 0.4f, 0.0f);
                body->m_Rotation = dmVMath::Quat::identity();
                body->m_Callbacks = &callbacks;
                dmPhysics::CollisionObjectData data;
                data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
                uint32_t shape_index = 1;
                if (i == 0)
                {
                    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
                    shape_index = 0;
                }
                else if (i == 4)
                {
                    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
                    shape_index = 2;
                }
                data.m_Mass = shape_index == 1 ? 1.0f : 0.0f;
                data.m_UserData = body;
                objects[i] = dmPhysics::NewCollisionObject2D(world, data, &shapes[shape_index], 1);
            }
            dmPhysics::StepWorldContext step;
            step.m_DT = 1.0f / 60.0f;
            step.m_Box2DSubStepCount = 4;
            step.m_ContactCallbacksReadOnly = read_only != 0;
            step.m_CollisionCallback = Collision;
            step.m_CollisionUserData = &callbacks;
            dmPhysics::StepWorld2D(world, step);
            ASSERT_GT(callbacks.m_Collisions, 3U);

            callbacks.m_Collisions = 0;
            callbacks.m_StopCallbacks = true;
            dmPhysics::StepWorld2D(world, step);
            ASSERT_EQ(3U, callbacks.m_Collisions);
            ASSERT_TRUE(callbacks.m_OnCallingThread);

            for (uint32_t i = 0; i < 5; ++i)
            {
                dmPhysics::DeleteCollisionObject2D(world, objects[i]);
            }
            for (uint32_t i = 0; i < 3; ++i)
            {
                dmPhysics::DeleteCollisionShape2D(shapes[i]);
            }
            dmPhysics::DeleteWorld2D(context, world);
            dmPhysics::DeleteContext2D(context);
        }
    }
    dmThread::FreeTls(key);
}

// Immediate callbacks must observe velocity changes made by earlier callbacks, even with workers enabled.
TEST(Box2DThreading, ImmediateCallbacksCanChangeVelocity)
{
    ASSERT_FALSE(dmPhysics::StepWorldContext().m_ContactCallbacksReadOnly);
    CompareThreadedCallbacks(false, false, true);
}

// Deleting a context with live worlds must release native worlds and join their workers.
TEST(Box2DThreading, ContextDeletesWorldsAndWorkers)
{
    size_t initial_bytes = b2GetByteCount();
    dmPhysics::HContext2D context = 0;
    CreateContext(3, &context);
    dmPhysics::HWorld2D world_a = dmPhysics::NewWorld2D(context, dmPhysics::NewWorldParams());
    dmPhysics::HWorld2D world_b = dmPhysics::NewWorld2D(context, dmPhysics::NewWorldParams());
    b2WorldId ids[] = { world_a->m_WorldId, world_b->m_WorldId };
    dmPhysics::DeleteContext2D(context);
    ASSERT_FALSE(b2World_IsValid(ids[0]));
    ASSERT_FALSE(b2World_IsValid(ids[1]));
    ASSERT_EQ(initial_bytes, b2GetByteCount());
}

#if defined(DM_HAS_THREADS) && (defined(_WIN32) || defined(__linux__) || defined(__APPLE__))
static uint32_t g_ThreadsCreated;
static uint32_t g_ThreadsJoined;

static void* CreateTrackedThread(void (*function)(void*), void* context, const char* name)
{
    ++g_ThreadsCreated;
    dmThread::Thread* thread = new dmThread::Thread;
    *thread = dmThread::New(function, 0x80000, context, name);
    return thread;
}

static void JoinTrackedThread(void* handle)
{
    ++g_ThreadsJoined;
    dmThread::Thread* thread = (dmThread::Thread*)handle;
    dmThread::Join(*thread);
    delete thread;
}

// The native scheduler must use the supplied thread interface and retain its own copy for cleanup.
TEST(Box2DThreading, SchedulerThreadInterface)
{
    g_ThreadsCreated = 0;
    g_ThreadsJoined = 0;
    b2SchedulerThreadInterface thread_interface = { CreateTrackedThread, JoinTrackedThread };
    b2Scheduler* scheduler = b2CreateScheduler(4, &thread_interface);
    ASSERT_EQ(3U, g_ThreadsCreated);
    thread_interface.createThread = 0;
    thread_interface.joinThread = 0;
    b2DestroyScheduler(scheduler);
    ASSERT_EQ(3U, g_ThreadsJoined);
}

struct WorkerGate
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t                                m_Entered;
    bool                                    m_Release;
};

static void WaitAtGate(void* data)
{
    WorkerGate* gate = (WorkerGate*)data;
    dmMutex::Lock(gate->m_Mutex);
    ++gate->m_Entered;
    dmConditionVariable::Broadcast(gate->m_Condition);
    while (!gate->m_Release)
    {
        dmConditionVariable::Wait(gate->m_Condition, gate->m_Mutex);
    }
    dmMutex::Unlock(gate->m_Mutex);
}

static void CountTask(void* data)
{
    dmAtomicIncrement32((int32_atomic_t*)data);
}

// All workers must run, and Finish must execute queued work when every background worker is occupied.
TEST(Box2DThreading, CallingThreadHelpsBusyWorkers)
{
    b2Scheduler* context = dmPhysics::NewBox2DScheduler(3);
    WorkerGate gate = { dmMutex::New(), dmConditionVariable::New(), 0, false };
    void* tasks[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        tasks[i] = b2SchedulerEnqueueTask(WaitAtGate, &gate, context);
    }
    dmMutex::Lock(gate.m_Mutex);
    while (gate.m_Entered != 3)
    {
        dmConditionVariable::Wait(gate.m_Condition, gate.m_Mutex);
    }
    dmMutex::Unlock(gate.m_Mutex);

    int32_atomic_t completed = 0;
    void* task = b2SchedulerEnqueueTask(CountTask, (void*)&completed, context);
    b2SchedulerFinishTask(task, context);
    ASSERT_EQ(1, dmAtomicGet32(&completed));

    dmMutex::Lock(gate.m_Mutex);
    gate.m_Release = true;
    dmConditionVariable::Broadcast(gate.m_Condition);
    dmMutex::Unlock(gate.m_Mutex);
    for (uint32_t i = 0; i < 3; ++i)
    {
        b2SchedulerFinishTask(tasks[i], context);
    }
    b2DestroyScheduler(context);
    dmConditionVariable::Delete(gate.m_Condition);
    dmMutex::Delete(gate.m_Mutex);
}

// Maximum-size batches and reverse-order waits must execute each task once and recycle every handle.
TEST(Box2DThreading, TaskCapacityAndReuse)
{
    b2Scheduler* context = dmPhysics::NewBox2DScheduler(3);
    int32_atomic_t counts[B2_MAX_TASKS] = {};
    void* tasks[B2_MAX_TASKS];
    for (uint32_t batch = 0; batch < 100; ++batch)
    {
        b2ResetScheduler(context);
        for (uint32_t i = 0; i < B2_MAX_TASKS; ++i)
        {
            tasks[i] = b2SchedulerEnqueueTask(CountTask, (void*)&counts[i], context);
        }
        for (uint32_t i = B2_MAX_TASKS; i > 0; --i)
        {
            b2SchedulerFinishTask(tasks[i - 1], context);
        }
        for (uint32_t i = 0; i < B2_MAX_TASKS; ++i)
        {
            ASSERT_EQ((int32_t)batch + 1, dmAtomicGet32(&counts[i]));
        }
    }
    b2DestroyScheduler(context);
}
#endif

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
