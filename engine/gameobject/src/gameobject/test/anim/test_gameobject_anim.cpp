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

#include <jc_test/jc_test.h>

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/easing.h>
#include <dlib/path.h>
#include <dlib/testutil.h>
#include <dlib/time.h>

#include "../../gameobject.h"
#include "../../gameobject_private.h"

using dmGameObject::CancelAnimations;

class AnimTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f ;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        char path[DMPATH_MAX_PATH];
        m_Factory = dmResource::NewFactory(&params, dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/gameobject/test/anim"));
        dmScript::ContextParams script_context_params = {};
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);

        m_Contexts.SetCapacity(7,16);
        m_Contexts.Put(dmHashString64("goc"), m_Register);
        m_Contexts.Put(dmHashString64("collectionc"), m_Register);
        m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
        m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
        dmResource::RegisterTypes(m_Factory, &m_Contexts);

        dmGameObject::ComponentTypeCreateCtx component_create_ctx = {};
        component_create_ctx.m_Script = m_ScriptContext;
        component_create_ctx.m_Register = m_Register;
        component_create_ctx.m_Factory = m_Factory;
        dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        dmGameObject::SortComponentTypes(m_Register);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, dmGameObject::GetCollectionDefaultCapacity(m_Register), 0x0);
        m_FinishCount = 0;
        m_CancelCount = 0;
    }

    void TearDown() override
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeregisterTypes(m_Factory, &m_Contexts);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
    uint32_t m_FinishCount;
    uint32_t m_CancelCount;
};

#define EPSILON 0.000001f

void AnimationStopped(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                    bool finished, void* userdata1, void* userdata2)
{
    AnimTest* test = (AnimTest*)userdata1;
    if (finished)
        ++test->m_FinishCount;
    else
        ++test->m_CancelCount;
}

static float X(dmGameObject::HInstance instance)
{
    return dmGameObject::GetPosition(instance).getX();
}

static dmhash_t hash(const char* s)
{
    return dmHashString64(s);
}

TEST_F(AnimTest, AnimateAndStop)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    ASSERT_NE((dmGameObject::HInstance)0, go);

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(dmVMath::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;
    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id,
            dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR),
            duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(2.5f, X(go), EPSILON);
    ASSERT_NEAR(2.5f, dmGameObject::GetWorldPosition(go).getX(), EPSILON);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(5.0f, X(go), EPSILON);
    ASSERT_NEAR(5.0f, dmGameObject::GetWorldPosition(go).getX(), EPSILON);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(7.5f, X(go), EPSILON);
    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(0u, this->m_CancelCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(10.0f, X(go), EPSILON);
    ASSERT_EQ(1u, this->m_FinishCount);
    ASSERT_EQ(0u, this->m_CancelCount);
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Playback)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(dmVMath::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    dmVMath::Point3 pos;

#define ANIM(playback)\
    Animate(m_Collection, go, 0, id, playback, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);

#define ASSERT_FRAME(expected)\
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));\
    ASSERT_NEAR(expected, X(go), EPSILON);

    ANIM(dmGameObject::PLAYBACK_ONCE_FORWARD);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(10.0f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_ONCE_BACKWARD);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_ONCE_PINGPONG);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_NONE);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_FORWARD);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(2.5f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_BACKWARD);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(7.5f);

    dmGameObject::SetPosition(go, dmVMath::Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_PINGPONG);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);

    ASSERT_EQ(3u, this->m_FinishCount);
    ASSERT_EQ(3u, this->m_CancelCount);

#undef ANIM
#undef ASSERT_FRAME

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Cancel)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(dmVMath::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, go, 0, id));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(1u, this->m_CancelCount);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, CancelAll)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id1 = hash("position");
    dmhash_t id2 = hash("scale");
    dmGameObject::PropertyVar var1(dmVMath::Vector3(10.f, 0.f, 0.f));
    dmGameObject::PropertyVar var2(2.f);
    float duration = 1.0f;
    float delay = 0.f;

    Animate(m_Collection, go, 0, id1, dmGameObject::PLAYBACK_ONCE_FORWARD, var1, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    Animate(m_Collection, go, 0, id2, dmGameObject::PLAYBACK_ONCE_FORWARD, var2, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, go, 0, 0));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));
    ASSERT_EQ(1.0f, dmGameObject::GetUniformScale(go));

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(2u, this->m_CancelCount);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, AnimateEuler)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("euler.z");
    dmGameObject::PropertyVar var(360.0f);
    float duration = 1.0f;
    float delay = 0.f;
    // Higher epsilon because of low precision euler conversions
    const float epsilon = 0.0001f;
    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id,
            dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR),
            duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    dmVMath::Quat r;

#define ASSERT_FRAME(q_z, q_w)\
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));\
    r = dmGameObject::GetRotation(go);\
    ASSERT_NEAR(q_z, r.getZ(), epsilon);\
    ASSERT_NEAR(q_w, r.getW(), epsilon);

    ASSERT_FRAME(M_SQRT1_2, M_SQRT1_2);

    ASSERT_FRAME(1, 0.0f);

    ASSERT_FRAME(M_SQRT1_2, -M_SQRT1_2);

    ASSERT_FRAME(0.0f, -1);

#undef ASSERT_FRAME
}

struct EulerCallbackState
{
    dmGameObject::HCollection m_Collection;
    dmGameObject::HInstance m_Other;
    dmVMath::Quat m_Rotation;
    dmVMath::Quat m_OtherRotation;
    uint32_t m_Count;
    bool m_DirtyTransforms;
};

static void ReadEulerCallback(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                             bool finished, void* userdata1, void* userdata2)
{
    EulerCallbackState* state = (EulerCallbackState*)userdata1;
    state->m_Rotation = dmGameObject::GetRotation(instance);
    state->m_OtherRotation = dmGameObject::GetRotation(state->m_Other);
    state->m_DirtyTransforms = dmGameObject::GetCollectionFromHandle(state->m_Collection)->m_DirtyTransforms;
    ++state->m_Count;
}

// All evaluated Euler axes and other objects must be committed before the first completion callback.
TEST_F(AnimTest, EulerCommittedBeforeCallbacks)
{
    dmGameObject::HInstance first = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::HInstance second = dmGameObject::New(m_Collection, "/dummy.goc");
    EulerCallbackState state = {};
    state.m_Collection = m_Collection;
    state.m_Other = second;
    m_UpdateContext.m_DT = 0.5f;
    dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
    dmGameObject::PropertyVar first_target(dmVMath::Vector3(20, 30, 40));
    dmGameObject::PropertyVar second_target(180.0f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, first, 0, hash("euler"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, first_target,
        easing, 0.5f, 0, ReadEulerCallback, &state, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, second, 0, hash("euler.z"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, second_target,
        easing, 0.5f, 0, 0, 0, 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1u, state.m_Count);
    ASSERT_TRUE(state.m_DirtyTransforms);
    dmVMath::Quat expected = dmVMath::EulerToQuat(dmVMath::Vector3(20, 30, 40));
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_NEAR(expected.getElem(i), state.m_Rotation.getElem(i), 0.0001f);
    ASSERT_NEAR(1.0f, state.m_OtherRotation.getZ(), 0.0001f);
    ASSERT_NEAR(0.0f, state.m_OtherRotation.getW(), 0.0001f);
}

// A first scalar Euler animation must retain unanimated axes without a getter allocating its record.
TEST_F(AnimTest, EulerAnimationStartsFromUncachedRotation)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, 0);
    dmVMath::Quat rotation = dmVMath::EulerToQuat(dmVMath::Vector3(20, 30, 40));
    dmGameObject::SetRotation(go, rotation);
    dmVMath::Vector3 initial = dmVMath::QuatToEuler(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    dmGameObject::PropertyVar target(80.0f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::Animate(m_Collection, go, 0,
        dmHashString64("euler.z"), dmGameObject::PLAYBACK_ONCE_FORWARD, target,
        dmEasing::Curve(dmEasing::TYPE_LINEAR), 0.5f, 0, 0, 0, 0));
    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    initial.setZ((initial.getZ() + 80.0f) * 0.5f);
    dmVMath::Quat expected = dmVMath::EulerToQuat(initial);
    dmVMath::Quat actual = dmGameObject::GetRotation(go);
    for (uint32_t axis = 0; axis < 4; ++axis)
        ASSERT_NEAR(expected.getElem(axis), actual.getElem(axis), 0.0001f);
}

// Separate axis tracks share one Euler value, including delayed writes and angles beyond one turn.
TEST_F(AnimTest, EulerDelayedAxesAndCancellation)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    m_UpdateContext.m_DT = 0.25f;
    dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
    dmGameObject::PropertyVar target_x(120.0f);
    dmGameObject::PropertyVar target_y(60.0f);
    dmGameObject::PropertyVar target_z(720.0f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, go, 0, hash("euler.x"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target_x, easing, 1, 0, 0, 0, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, go, 0, hash("euler.y"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target_y, easing, 1, 0.25f, 0, 0, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, go, 0, hash("euler.z"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target_z, easing, 1, 0, 0, 0, 0));

    for (uint32_t frame = 1; frame <= 2; ++frame)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        dmVMath::Quat expected = dmVMath::EulerToQuat(dmVMath::Vector3(30.0f * frame, 15.0f * (frame - 1), 180.0f * frame));
        dmVMath::Quat actual = dmGameObject::GetRotation(go);
        for (uint32_t i = 0; i < 4; ++i)
            ASSERT_NEAR(expected.getElem(i), actual.getElem(i), 0.0001f);
    }
    // Cancellation queries the property and normalizes its Euler representation from the quaternion.
    dmVMath::Quat before_cancel = dmGameObject::GetRotation(go);
    dmVMath::Vector3 retained = dmVMath::QuatToEuler(before_cancel.getX(), before_cancel.getY(), before_cancel.getZ(), before_cancel.getW());
    CancelAnimations(m_Collection, go, 0, hash("euler.x"));
    CancelAnimations(m_Collection, go, 0, hash("euler.y"));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    retained.setZ(540);
    dmVMath::Quat expected = dmVMath::EulerToQuat(retained);
    dmVMath::Quat actual = dmGameObject::GetRotation(go);
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_NEAR(expected.getElem(i), actual.getElem(i), 0.0001f);
}

// Euler writeback only overrides quaternion animation when an Euler value changes, in either creation order.
TEST_F(AnimTest, EulerWritebackOnlyForChangedValues)
{
    dmGameObject::HInstance objects[4];
    dmVMath::Quat target_rotation = dmVMath::EulerToQuat(dmVMath::Vector3(90, 0, 0));
    dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
    for (uint32_t i = 0; i < 4; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        dmhash_t properties[2] = { hash("euler.z"), hash("rotation") };
        dmGameObject::PropertyVar targets[2] = {
            dmGameObject::PropertyVar((i & 1) ? 90.0f : 0.0f),
            dmGameObject::PropertyVar(target_rotation)
        };
        for (uint32_t j = 0; j < 2; ++j)
        {
            uint32_t property = (i & 2) ? 1 - j : j;
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, properties[property],
                dmGameObject::PLAYBACK_ONCE_FORWARD, targets[property], easing, 1, 0, 0, 0, 0));
        }
    }
    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmVMath::Quat expected = (i & 1)
            ? dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, 22.5f))
            : dmVMath::Quat(target_rotation.getX() * 0.25f, 0, 0, 1 + (target_rotation.getW() - 1) * 0.25f);
        dmVMath::Quat actual = dmGameObject::GetRotation(objects[i]);
        for (uint32_t j = 0; j < 4; ++j)
            ASSERT_NEAR(expected.getElem(j), actual.getElem(j), 0.0001f);
    }
}

void AnimationStoppedToDelete(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                    bool finished, void* userdata1, void* userdata2)
{
    AnimTest* test = (AnimTest*)userdata1;
    if (finished)
        ++test->m_FinishCount;
    else
        ++test->m_CancelCount;
    *((dmhash_t*)userdata2) = dmGameObject::GetIdentifier(instance);
}

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, dmGameObject::HPropertyContainer properties, const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, properties, position, rotation, scale);
        dmResource::Release(factory, prototype);
        return result;
    }
    return 0x0;
}

TEST_F(AnimTest, DeleteInAnim)
{
    const uint32_t instance_count = 3;
    dmGameObject::HInstance gos[instance_count];
    dmhash_t orig_instance_ids[instance_count];
    uint32_t order[instance_count] = {1, 0, 2};
    char id_buffer[32];
    const char* id_fmt = "test_id%d";
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        gos[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        dmSnPrintf(id_buffer, 32, id_fmt, i);
        orig_instance_ids[i] = dmHashString64(id_buffer);
        dmGameObject::SetIdentifier(m_Collection, gos[i], id_buffer);
    }

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(dmVMath::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;
    dmhash_t instance_id = 0;

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        Animate(m_Collection, gos[i], 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStoppedToDelete, this, &instance_id);
    }

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);

        dmGameObject::Delete(m_Collection, gos[order[i]], false);

        dmGameObject::PostUpdate(m_Collection);

        ASSERT_EQ(0u, this->m_FinishCount);
        ASSERT_EQ(i+1, this->m_CancelCount);
        ASSERT_EQ(orig_instance_ids[order[i]], instance_id);
    }
    dmGameObject::Update(m_Collection, &m_UpdateContext);
}

// Tests that animation with duration=0 is equivalent with "set" of the target value
TEST_F(AnimTest, ZeroDuration)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 0.0f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(10.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Delay)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 1.0f;
    float delay = 1.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmVMath::Point3 pos;

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_LT(0.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, DelayAboveDuration)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 1.0f;
    float delay = 2.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmVMath::Point3 pos;

    for (uint32_t i = 0; i < 8; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_EQ(0.0f, X(go));
    }

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_LT(0.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

// Test that a delayed animation is not stopped when a new is started immediately
TEST_F(AnimTest, DelayedNotStopped)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var_delay(1.f);
    dmGameObject::PropertyVar var_immediate(0.5f);
    float duration = 0.75f;
    float delay = 0.25f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var_delay, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var_immediate, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, 0.0f, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmVMath::Point3 pos;

    for (uint32_t i = 0; i < 4; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_LT(0.0f, X(go));
    }

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(1.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

// Growing and recycling Euler storage must preserve running bindings and reset records for new instance handles.
TEST_F(AnimTest, EulerPoolGrowthAndReuse)
{
    const uint32_t count = 768;
    dmGameObject::HInstance objects[count];
    m_UpdateContext.m_DT = 0.25f;
    dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
    dmGameObject::PropertyVar target(720.0f);
    objects[0] = dmGameObject::New(m_Collection, "/dummy.goc");
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[0], 0, hash("euler.z"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target, easing, 1, 0, 0, 0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 1; i < count; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, objects[i]);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("euler.z"),
            dmGameObject::PLAYBACK_ONCE_FORWARD, target, easing, 1, 0, 0, 0, 0));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < count; ++i)
    {
        dmVMath::Quat expected = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, i == 0 ? 360 : 180));
        dmVMath::Quat actual = dmGameObject::GetRotation(objects[i]);
        for (uint32_t axis = 0; axis < 4; ++axis)
            ASSERT_NEAR(expected.getElem(axis), actual.getElem(axis), 0.0001f);
    }
    for (uint32_t i = 0; i < count; i += 2)
        dmGameObject::Delete(m_Collection, objects[i], false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PropertyVar replacement_target(180.0f);
    for (uint32_t i = 0; i < count; i += 2)
    {
        dmGameObject::HInstance old_handle = objects[i];
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE(old_handle, objects[i]);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("euler.z"),
            dmGameObject::PLAYBACK_ONCE_FORWARD, replacement_target, easing, 1, 0, 0, 0, 0));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < count; ++i)
    {
        dmVMath::Quat expected = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, (i & 1) ? 360 : 45));
        dmVMath::Quat actual = dmGameObject::GetRotation(objects[i]);
        for (uint32_t axis = 0; axis < 4; ++axis)
            ASSERT_NEAR(expected.getElem(axis), actual.getElem(axis), 0.0001f);
    }
}

struct EulerPoolCallbackState
{
    AnimTest* m_Test;
    dmGameObject::HInstance m_Objects[700];
    bool m_Called;
};

static void GrowEulerPoolCallback(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
    bool finished, void* userdata1, void* userdata2)
{
    EulerPoolCallbackState* state = (EulerPoolCallbackState*)userdata1;
    ASSERT_TRUE(finished);
    state->m_Called = true;
    dmGameObject::PropertyVar target(360.0f);
    for (uint32_t i = 0; i < 700; ++i)
    {
        state->m_Objects[i] = dmGameObject::New(state->m_Test->m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, state->m_Objects[i]);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(state->m_Test->m_Collection, state->m_Objects[i], 0,
            hash("euler.z"), dmGameObject::PLAYBACK_ONCE_FORWARD, target,
            dmEasing::Curve(dmEasing::TYPE_LINEAR), 1, 0, 0, 0, 0));
    }
    dmGameObject::PropertyOptions options;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(instance, 0, hash("euler.x"), options,
        dmGameObject::PropertyVar(60.0f)));
}

// Completion callbacks may grow both pools and then access the completed object's retained Euler axes.
TEST_F(AnimTest, EulerPoolGrowthDuringCallback)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    m_UpdateContext.m_DT = 0.25f;
    EulerPoolCallbackState state = {};
    state.m_Test = this;
    dmGameObject::PropertyVar target(450.0f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, go, 0, hash("euler.z"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR),
        0.25f, 0, GrowEulerPoolCallback, &state, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(state.m_Called);
    dmVMath::Quat expected = dmVMath::EulerToQuat(dmVMath::Vector3(60, 0, 450));
    for (uint32_t axis = 0; axis < 4; ++axis)
        ASSERT_NEAR(expected.getElem(axis), dmGameObject::GetRotation(go).getElem(axis), 0.0001f);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    expected = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, 90));
    for (uint32_t i = 0; i < 700; ++i)
        for (uint32_t axis = 0; axis < 4; ++axis)
            ASSERT_NEAR(expected.getElem(axis), dmGameObject::GetRotation(state.m_Objects[i]).getElem(axis), 0.0001f);
}

TEST_F(AnimTest, LoadTest)
{
    const uint32_t count = 1024;
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(dmVMath::Vector3(10.0f, 0.0f, 0.0f));
    float duration = 1.0f;
    float delay = 0.0f;

    dmGameObject::HInstance gos[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        gos[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        dmGameObject::PropertyResult result = Animate(m_Collection, gos[i], 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    }

    uint64_t time = dmTime::GetMonotonicTime();
    dmGameObject::Update(m_Collection, &m_UpdateContext);
    uint64_t delta = dmTime::GetMonotonicTime() - time;

    printf("%d animations started in %.3f ms\n", count*4, delta * 0.001);

    time = dmTime::GetMonotonicTime();
    dmGameObject::Update(m_Collection, &m_UpdateContext);
    delta = dmTime::GetMonotonicTime() - time;

    printf("%d animations simulated in %.3f ms\n", count*3, delta * 0.001);

    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::Delete(m_Collection, gos[i], false);
    }
}

TEST_F(AnimTest, LinkedList)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    float duration = 1.0f;
    float delay = 0.0f;
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmhash_t ids[] = {hash("position.x"), hash("position.y"), hash("position.z")};
    dmVMath::Point3 p;

#define ANIM\
    dmGameObject::SetPosition(go, dmVMath::Point3(0, 0, 0));\
    Animate(m_Collection, go, 0, ids[0], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[1], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[2], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
#define ASSERT_FRAME(x, y, z)\
    dmGameObject::Update(m_Collection, &m_UpdateContext);\
    p = dmGameObject::GetPosition(go);\
    ASSERT_NEAR(x, p.getX(), EPSILON);\
    ASSERT_NEAR(y, p.getY(), EPSILON);\
    ASSERT_NEAR(z, p.getZ(), EPSILON);

    // Cancel head
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[0]);
    ASSERT_FRAME(0, 0.25, 0.25);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0, 0.25, 0.25);

    // Cancel middle
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[1]);
    ASSERT_FRAME(0.25, 0, 0.25);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0.25, 0, 0.25);

    // Cancel tail
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[2]);
    ASSERT_FRAME(0.25, 0.25, 0);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0.25, 0.25, 0);

#undef ANIM
#undef ASSERT_FRAME
}

TEST_F(AnimTest, ScriptedRestart)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/restart.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedCancel)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/cancel.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedCancelAll)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/cancel_all.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedAnimBadURL)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/anim_bad_url.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_EQ(0, go);
}

TEST_F(AnimTest, ScriptedCancelBadURL)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/cancel_bad_url.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_EQ(0, go);
}

TEST_F(AnimTest, ScriptedChainOtherProp)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chain_other_prop.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedChainDelayBug)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chain_delay_bug.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

// Test that relocating anims through a SetCapacity (via callback) works
TEST_F(AnimTest, ScriptedDemo)
{
    uint32_t count = 257;
    char id[8];
    for (uint32_t i = 0; i < count; ++i)
    {
        dmSnPrintf(id, 8, "box%d", i + 1);
        dmGameObject::HInstance box = Spawn(m_Factory, m_Collection, "/demo_box.goc", hash(id), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
        ASSERT_NE(0, box);
    }
    dmGameObject::HInstance demo = Spawn(m_Factory, m_Collection, "/demo.goc", hash("demo"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, demo);

    uint32_t frame_count = 1000;
    m_UpdateContext.m_DT = 0.25f;

    for (uint32_t i = 0; i < frame_count; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedInvalidType)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/invalid_type.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_EQ(0, go);
}

TEST_F(AnimTest, ScriptedDelayedCompositeCallback)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/composite_delay.goc", hash("test"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}


TEST_F(AnimTest, ScriptedCustomEasing)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/custom_easing.goc", hash("custom_easing"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedChainedEasing)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chained_easing.goc", hash("chained_easing"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (uint32_t i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, PositionUniformAnim)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetPosition(go, dmVMath::Point3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    dmVMath::Point3 position = dmGameObject::GetPosition(go);
    ASSERT_NEAR(2.0f, position.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, position.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, position.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

// Test that the 3 component scale can be animated as a uniform scale (legacy)
TEST_F(AnimTest, UniformScale)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, 1.0f);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(2.0f, dmGameObject::GetUniformScale(go), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleUniformAnim)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, dmVMath::Vector3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    dmVMath::Vector3 scale = dmGameObject::GetScale(go);
    ASSERT_NEAR(2.0f, scale.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Scale)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, dmVMath::Vector3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(dmVMath::Vector3(2.f));
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    dmVMath::Vector3 scale = dmGameObject::GetScale(go);
    ASSERT_NEAR(2.0f, scale.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleX)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.x");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleY)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.y");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleZ)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.z");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}
