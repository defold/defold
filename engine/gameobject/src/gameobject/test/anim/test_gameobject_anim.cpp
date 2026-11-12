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
#include <stdlib.h>

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

static void CountReleasedCurve(dmEasing::Curve* curve)
{
    ++*(uint32_t*)curve->userdata1;
}

// Callback-free tracks still release owned curves once, on completion, cancellation, and deletion.
TEST_F(AnimTest, CallbackFreeCurveLifetime)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    uint32_t released = 0;
    dmVMath::FloatVector values(2);
    values.values[0] = 0;
    values.values[1] = 1;
    dmEasing::Curve curve(dmEasing::TYPE_FLOAT_VECTOR);
    curve.vector = &values;
    curve.release_callback = CountReleasedCurve;
    curve.userdata1 = &released;
    m_UpdateContext.m_DT = 0.25f;
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        dmGameObject::PropertyVar target(10.0f);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, go, 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_ONCE_FORWARD, target, curve, 0.5f, 0, 0, 0, 0));
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_EQ(pass, released);
        if (pass == 0)
        {
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        }
        else if (pass == 1)
        {
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, go, 0, hash("position.x")));
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        }
        else
        {
            dmGameObject::Delete(m_Collection, go, false);
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        }
        ASSERT_EQ(pass + 1, released);
    }
}

// Switch between scalar-only and SIMD-eligible collections without losing starts,
// final values, or one-shot completion notifications.
TEST_F(AnimTest, EvaluationPassSwitchesBetweenScalarAndSimd)
{
    dmGameObject::HInstance objects[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
    }
    m_UpdateContext.m_DT = 0.25f;
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        const bool loop = pass == 1;
        const dmGameObject::Playback playback = loop ? dmGameObject::PLAYBACK_LOOP_FORWARD : dmGameObject::PLAYBACK_ONCE_FORWARD;
        const float from = pass * 10.0f;
        dmGameObject::PropertyVar target(from + 10.0f);
        for (uint32_t i = 0; i < 4; ++i)
        {
            // Capture the new start value after the previous pass has stopped.
            dmGameObject::SetPosition(objects[i], dmVMath::Point3(from, 0, 0));
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
                                                                playback, target, dmEasing::Curve(dmEasing::TYPE_LINEAR),
                                                                1.0f, 0, AnimationStopped, this, 0));
        }
        for (uint32_t frame = 0; frame < 4; ++frame)
        {
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            const float expected = loop && frame == 3 ? from : from + (frame + 1) * 2.5f;
            for (uint32_t i = 0; i < 4; ++i)
            {
                ASSERT_NEAR(expected, X(objects[i]), EPSILON);
            }
        }
        if (loop)
        {
            for (uint32_t i = 0; i < 4; ++i)
            {
                ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, objects[i], 0, hash("position.x")));
            }
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_EQ(pass == 2 ? 8u : 4u, m_FinishCount);
        ASSERT_EQ(pass == 0 ? 0u : 4u, m_CancelCount);
    }
}

// Compare dense batching with isolated scalar collections across wraps, delays,
// cancellations, replacements and deletion, including every built-in curve and tails.
TEST_F(AnimTest, BatchedLoopPropertiesMatchScalar)
{
    const uint32_t count = 43;
    dmGameObject::HCollection references[count];
    dmGameObject::HInstance objects[3][count];
    dmGameObject::HInstance controls[3][count];
    dmVMath::FloatVector samples(2);
    samples.values[0] = 0.0f;
    samples.values[1] = 1.0f;
    dmEasing::Curve custom(dmEasing::TYPE_FLOAT_VECTOR);
    custom.vector = &samples;
    dmGameObject::PropertyVar zero(0.0f);
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        dmGameObject::Playback playback = (dmGameObject::Playback)(dmGameObject::PLAYBACK_LOOP_FORWARD + mode);
        for (uint32_t i = 0; i < count; ++i)
        {
            objects[mode][i] = dmGameObject::New(m_Collection, "/dummy.goc");
            if (mode == 0)
            {
                char name[64];
                dmSnPrintf(name, sizeof(name), "scalar-reference-%u", i);
                references[i] = dmGameObject::NewCollection(name, m_Factory, m_Register, 8, 0);
                ASSERT_NE((dmGameObject::HCollection)0, references[i]);
            }
            controls[mode][i] = dmGameObject::New(references[i], "/dummy.goc");
            ASSERT_NE((dmGameObject::HInstance)0, objects[mode][i]);
            ASSERT_NE((dmGameObject::HInstance)0, controls[mode][i]);
            dmVMath::Point3 position((float)i * -0.125f, 0.0f, 0.0f);
            dmGameObject::SetPosition(objects[mode][i], position);
            dmGameObject::SetPosition(controls[mode][i], position);
            dmGameObject::PropertyVar target(20.0f + i);
            dmEasing::Curve curve((dmEasing::Type)(i % dmEasing::TYPE_FLOAT_VECTOR));
            float duration = i == 17 ? 0.0f : (1 + i % 19) * 0.015625f;
            float delay = i % 13 == 12 ? 0.375f : 0.0f;
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[mode][i], 0, hash("position.x"),
                                                                playback, target, curve, duration, delay, AnimationStopped, this, 0));
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[mode][i], 0, hash("position.x"),
                                                                playback, target, curve, duration, delay, 0, 0, 0));
            // Each reference has at most three SIMD-eligible tracks, so evaluation cannot form a four-track batch.
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[mode][i], 0, hash("position.z"),
                                                                playback, zero, custom, 1.0f, 0, 0, 0, 0));
        }
    }
    const float steps[] = { 0.0f, 0.01f, 0.75f, 1.0f / 60.0f, -0.125f, 0.25f };
    for (uint32_t frame = 0; frame < 120; ++frame)
    {
        if (frame == 24)
        {
            for (uint32_t mode = 0; mode < 3; ++mode)
            {
                for (uint32_t i = 0; i < count; i += 5)
                {
                    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, objects[mode][i], 0, hash("position.x")));
                    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(references[i], controls[mode][i], 0, hash("position.x")));
                }
            }
        }
        if (frame == 48)
        {
            dmGameObject::PropertyVar target(-5.0f);
            for (uint32_t mode = 0; mode < 3; ++mode)
            {
                for (uint32_t i = 0; i < count; i += 7)
                {
                    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[mode][i], 0, hash("position.x"),
                                                                        dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 0.125f, 0.075f, AnimationStopped, this, 0));
                    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[mode][i], 0, hash("position.x"),
                                                                        dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 0.125f, 0.075f, 0, 0, 0));
                }
            }
        }
        if (frame == 72)
        {
            for (uint32_t mode = 0; mode < 3; ++mode)
            {
                dmGameObject::Delete(m_Collection, objects[mode][5], false);
                dmGameObject::Delete(references[5], controls[mode][5], false);
                ASSERT_TRUE(dmGameObject::PostUpdate(references[5]));
            }
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        }
        m_UpdateContext.m_DT = steps[frame % DM_ARRAY_SIZE(steps)];
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_TRUE(dmGameObject::Update(references[i], &m_UpdateContext));
        }
        for (uint32_t mode = 0; mode < 3; ++mode)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                if (i != 5 || frame < 72)
                {
                    ASSERT_EQ(X(controls[mode][i]), X(objects[mode][i]));
                }
            }
        }
    }
    ASSERT_EQ(21u, m_FinishCount);
    ASSERT_EQ(42u, m_CancelCount);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::DeleteCollection(references[i]);
    }
    dmGameObject::PostUpdate(m_Register);
}

// Whole Vector3 batches include their parent lane. Compare against isolated scalar
// axes through mixed curves, delays, wraps, negative dt and channel replacement.
TEST_F(AnimTest, BatchedVector3MatchesScalarAxes)
{
    const uint32_t count = dmEasing::TYPE_FLOAT_VECTOR * 3;
    dmGameObject::HCollection references[count];
    dmGameObject::HInstance objects[count];
    dmGameObject::HInstance controls[count];
    const char* channels[] = { "position.x", "position.y", "position.z" };
    for (uint32_t i = 0; i < count; ++i)
    {
        char name[64];
        dmSnPrintf(name, sizeof(name), "vector-reference-%u", i);
        references[i] = dmGameObject::NewCollection(name, m_Factory, m_Register, 8, 0);
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        controls[i] = dmGameObject::New(references[i], "/dummy.goc");
        dmVMath::Point3 from((float)i, -20.0f, 3.0f);
        dmGameObject::SetPosition(objects[i], from);
        dmGameObject::SetPosition(controls[i], from);
        dmGameObject::Playback playback = (dmGameObject::Playback)(dmGameObject::PLAYBACK_LOOP_FORWARD + i % 3);
        dmEasing::Curve curve((dmEasing::Type)(i / 3));
        float duration = (1 + i % 19) * 0.015625f;
        float delay = i % 13 == 12 ? 0.375f : 0.0f;
        dmVMath::Vector3 to(30.0f, -5.0f, 11.0f);
        dmGameObject::PropertyVar vector_target(to);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position"),
                                                            playback, vector_target, curve, duration, delay, AnimationStopped, this, 0));
        // Three scalar tracks per collection cannot form a SIMD batch.
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            dmGameObject::PropertyVar axis_target(to.getElem(axis));
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[i], 0, hash(channels[axis]),
                                                                playback, axis_target, curve, duration, delay, 0, 0, 0));
        }
    }
    const float steps[] = { 0.0f, 0.01f, 0.75f, 1.0f / 60.0f, -0.125f, 0.25f };
    for (uint32_t frame = 0; frame < 96; ++frame)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (frame == 24)
            {
                CancelAnimations(m_Collection, objects[i], 0, hash("position.y"));
                CancelAnimations(references[i], controls[i], 0, hash("position.y"));
            }
            if (frame == 48)
            {
                dmGameObject::PropertyVar target(4.0f);
                ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.y"),
                                                                    dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 0.125f, 0.075f, 0, 0, 0));
                ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[i], 0, hash("position.y"),
                                                                    dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 0.125f, 0.075f, 0, 0, 0));
            }
            if (frame == 72)
            {
                CancelAnimations(m_Collection, objects[i], 0, hash("position"));
                CancelAnimations(references[i], controls[i], 0, 0);
            }
        }
        m_UpdateContext.m_DT = steps[frame % DM_ARRAY_SIZE(steps)];
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_TRUE(dmGameObject::Update(references[i], &m_UpdateContext));
            dmVMath::Point3 expected = dmGameObject::GetPosition(controls[i]);
            dmVMath::Point3 actual = dmGameObject::GetPosition(objects[i]);
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                ASSERT_EQ(expected.getElem(axis), actual.getElem(axis));
            }
        }
    }
    ASSERT_EQ(0u, m_FinishCount);
    ASSERT_EQ(count, m_CancelCount);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::DeleteCollection(references[i]);
    }
    dmGameObject::PostUpdate(m_Register);
}

// Pruning can move a delayed tail below the old activation bound; deleting an
// instance can similarly move a stopped tail below the old pruning bound.
TEST_F(AnimTest, LifecycleBoundsFollowSwapErasure)
{
    dmGameObject::HInstance objects[9];
    dmGameObject::PropertyVar target(20.0f);
    for (uint32_t i = 0; i < 9; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR),
                                                            1.0f, i == 8 ? 0.5f : 0.0f, AnimationStopped, this, 0));
    }
    m_UpdateContext.m_DT = 0.125f;
    CancelAnimations(m_Collection, objects[0], 0, hash("position.x"));
    for (uint32_t frame = 0; frame < 6; ++frame)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_EQ(frame < 4 ? 0.0f : (frame - 3) * 2.5f, X(objects[8]));
    }
    ASSERT_EQ(1u, m_CancelCount);
    // Index 7 is now the tail; removing index 1 moves that stopped tail down.
    CancelAnimations(m_Collection, objects[7], 0, hash("position.x"));
    dmGameObject::Delete(m_Collection, objects[1], false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(2u, m_CancelCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(3u, m_CancelCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(3u, m_CancelCount);
    ASSERT_EQ(0u, m_FinishCount);
}

// Delayed loops must become batchable after a scalar-only pass. Negative steps,
// cancellation and scalar completion must keep values equal to isolated tracks.
TEST_F(AnimTest, DelayedLoopsMatchScalarEvaluation)
{
    const uint32_t count = 13;
    dmGameObject::HCollection references[count];
    dmGameObject::HInstance objects[count];
    dmGameObject::HInstance controls[count];
    dmGameObject::PropertyVar target(16.0f);
    for (uint32_t i = 0; i < count; ++i)
    {
        char name[64];
        dmSnPrintf(name, sizeof(name), "delayed-reference-%u", i);
        references[i] = dmGameObject::NewCollection(name, m_Factory, m_Register, 8, 0);
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        controls[i] = dmGameObject::New(references[i], "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, objects[i]);
        ASSERT_NE((dmGameObject::HInstance)0, controls[i]);
        float delay = i < 8 ? 0.1875f : 0.4375f;
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_PINGPONG, target, dmEasing::Curve(dmEasing::TYPE_INOUTSINE), 1, delay, 0, 0, 0));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(references[i], controls[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_PINGPONG, target, dmEasing::Curve(dmEasing::TYPE_INOUTSINE), 1, delay, 0, 0, 0));
    }
    for (uint32_t frame = 0; frame < 24; ++frame)
    {
        // Scalar tracks start and finish alongside the looping tracks.
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[0], 0, hash("position.z"),
                                                            dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 0, 0, AnimationStopped, this, 0));
        if (frame == 12)
        {
            dmGameObject::CancelAnimations(m_Collection, objects[5], 0, hash("position.x"));
            dmGameObject::CancelAnimations(references[5], controls[5], 0, hash("position.x"));
        }
        m_UpdateContext.m_DT = frame == 8 ? -0.125f : 0.125f;
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_TRUE(dmGameObject::Update(references[i], &m_UpdateContext));
            ASSERT_EQ(X(controls[i]), X(objects[i]));
        }
    }
    ASSERT_EQ(24u, m_FinishCount);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::DeleteCollection(references[i]);
    }
    dmGameObject::PostUpdate(m_Register);
}

struct AnimationGroupCallbackState
{
    uint32_t m_Ids[12];
    uint32_t m_Count;
    uint32_t m_Canceled;
};

static void AnimationGroupStopped(dmGameObject::HInstance, dmhash_t, dmhash_t, bool finished, void* userdata1, void* userdata2)
{
    AnimationGroupCallbackState* state = (AnimationGroupCallbackState*)userdata1;
    if (finished)
    {
        state->m_Ids[state->m_Count++] = (uint32_t)(uintptr_t)userdata2;
    }
    else
    {
        ++state->m_Canceled;
    }
}

// Evaluating mixed SIMD, Euler and custom tracks must preserve the original
// swap-erasure callback order, including custom curves on Euler properties.
TEST_F(AnimTest, MixedAnimationsPreserveCallbackOrder)
{
    AnimationGroupCallbackState state = {};
    dmGameObject::HInstance objects[12];
    dmVMath::FloatVector samples(2);
    samples.values[0] = 0;
    samples.values[1] = 1;
    dmGameObject::PropertyVar target(8.0f);
    // Keep a full SIMD batch active while the mixed tracks below complete.
    dmGameObject::HInstance loop_objects[4];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(loop_objects); ++i)
    {
        loop_objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, loop_objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 1.0f, 0,
                                                            AnimationGroupStopped, &state, 0));
    }
    for (uint32_t i = 0; i < 12; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        const bool loop = i == 0 || i == 4 || i == 8 || i == 9;
        const bool euler = i == 1 || i == 5;
        dmEasing::Curve curve(dmEasing::TYPE_LINEAR);
        if (i == 2 || i == 5 || i == 6 || i == 10)
        {
            curve.type = dmEasing::TYPE_FLOAT_VECTOR;
            curve.vector = &samples;
        }
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0,
                                                            euler ? hash("euler.z") : hash("position.x"),
                                                            loop ? dmGameObject::PLAYBACK_LOOP_FORWARD : dmGameObject::PLAYBACK_ONCE_FORWARD,
                                                            target, curve, 0.125f, 0, AnimationGroupStopped, &state, (void*)(uintptr_t)i));
    }
    m_UpdateContext.m_DT = 0.0625f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(0u, state.m_Count);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(loop_objects); ++i)
    {
        ASSERT_EQ(1.0f, X(loop_objects[i]));
    }
    const uint32_t expected[] = { 1, 11, 10, 2, 3, 7, 6, 5 };
    ASSERT_EQ((uint32_t)DM_ARRAY_SIZE(expected), state.m_Count);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(expected); ++i)
    {
        ASSERT_EQ(expected[i], state.m_Ids[i]);
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((uint32_t)DM_ARRAY_SIZE(expected), state.m_Count);
    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, objects[i], 0, 0));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(4u, state.m_Canceled);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(loop_objects); ++i)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, loop_objects[i], 0, 0));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(8u, state.m_Canceled);
    ASSERT_EQ((uint32_t)DM_ARRAY_SIZE(expected), state.m_Count);
}

struct ExtrasPoolTestState
{
    dmGameObject::HInstance m_Objects[129];
    uint32_t                m_Finished[129];
    uint32_t                m_Canceled[129];
    uint32_t                m_Released[129];
};

static void ExtrasPoolStopped(dmGameObject::HInstance instance, dmhash_t, dmhash_t, bool finished, void* userdata1, void* userdata2)
{
    ExtrasPoolTestState* state = (ExtrasPoolTestState*)userdata1;
    uint32_t index = (uint32_t)(uintptr_t)userdata2;
    ASSERT_EQ(state->m_Objects[index], instance);
    if (finished)
    {
        ++state->m_Finished[index];
    }
    else
    {
        ++state->m_Canceled[index];
    }
}

static void ExtrasPoolReleased(dmEasing::Curve* curve)
{
    ExtrasPoolTestState* state = (ExtrasPoolTestState*)curve->userdata1;
    ++state->m_Released[(uint32_t)(uintptr_t)curve->userdata2];
}

// Sparse extras must stay attached to their owners after compaction and slot reuse.
TEST_F(AnimTest, AnimationExtrasPoolCompactionAndReuse)
{
    ExtrasPoolTestState state = {};
    dmVMath::FloatVector samples(2);
    samples.values[0] = 0.0f;
    samples.values[1] = 1.0f;
    dmGameObject::PropertyVar target(8.0f);
    m_UpdateContext.m_DT = 0.25f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); ++i)
    {
        state.m_Objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, state.m_Objects[i]);
        dmEasing::Curve curve(i % 3 == 2 ? dmEasing::TYPE_FLOAT_VECTOR : dmEasing::TYPE_LINEAR);
        if (i % 3 == 2)
        {
            curve.vector = &samples;
            curve.release_callback = ExtrasPoolReleased;
            curve.userdata1 = &state;
            curve.userdata2 = (void*)(uintptr_t)i;
        }
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, state.m_Objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_ONCE_FORWARD, target, curve, 1.0f, 0, i % 3 ? ExtrasPoolStopped : 0, &state, (void*)(uintptr_t)i));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); i += 2)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, state.m_Objects[i], 0, hash("position.x")));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    target = dmGameObject::PropertyVar(6.0f);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); i += 2)
    {
        ASSERT_EQ(i % 3 ? 1u : 0u, state.m_Canceled[i]);
        ASSERT_EQ(i % 3 == 2 ? 1u : 0u, state.m_Released[i]);
        dmEasing::Curve curve(dmEasing::TYPE_FLOAT_VECTOR);
        curve.vector = &samples;
        curve.release_callback = ExtrasPoolReleased;
        curve.userdata1 = &state;
        curve.userdata2 = (void*)(uintptr_t)i;
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, state.m_Objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_ONCE_FORWARD, target, curve, 0.5f, 0, ExtrasPoolStopped, &state, (void*)(uintptr_t)i));
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); ++i)
    {
        ASSERT_EQ((i & 1) ? 8.0f : 6.0f, X(state.m_Objects[i]));
        ASSERT_EQ((i & 1) && i % 3 == 0 ? 0u : 1u, state.m_Finished[i]);
        ASSERT_EQ(((i & 1) == 0 && i % 3) ? 1u : 0u, state.m_Canceled[i]);
        ASSERT_EQ((i % 3 == 2 ? 1u : 0u) + ((i & 1) ? 0u : 1u), state.m_Released[i]);
    }
}

struct ExtrasReleaseGrowthState
{
    AnimTest*               m_Test;
    dmGameObject::HInstance m_Objects[700];
    uint32_t                m_Released;
};

static void GrowExtrasDuringCurveRelease(dmEasing::Curve* curve)
{
    ExtrasReleaseGrowthState* state = (ExtrasReleaseGrowthState*)curve->userdata1;
    ++state->m_Released;
    dmGameObject::PropertyVar target(4.0f);
    dmEasing::Curve owned(dmEasing::TYPE_LINEAR);
    owned.release_callback = CountReleasedCurve;
    owned.userdata1 = &state->m_Released;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state->m_Objects); ++i)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(state->m_Test->m_Collection, state->m_Objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_ONCE_FORWARD, target, owned, 1.0f, 0, AnimationStopped, state->m_Test, 0));
    }
    // The argument must remain valid even after the pooled storage has moved.
    ASSERT_EQ((void*)state, curve->userdata1);
    ASSERT_EQ((void*)(uintptr_t)123, curve->userdata2);
    curve->release_callback = 0;
    curve->userdata1 = 0;
    curve->userdata2 = 0;
}

// Curve release may grow both pools; its argument and the owning animation must survive relocation.
TEST_F(AnimTest, AnimationExtrasGrowthDuringCurveRelease)
{
    ExtrasReleaseGrowthState state = {};
    state.m_Test = this;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); ++i)
    {
        state.m_Objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, state.m_Objects[i]);
    }
    dmGameObject::PropertyVar target(1.0f);
    dmEasing::Curve curve(dmEasing::TYPE_LINEAR);
    curve.release_callback = GrowExtrasDuringCurveRelease;
    curve.userdata1 = &state;
    curve.userdata2 = (void*)(uintptr_t)123;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, state.m_Objects[0], 0, hash("position.y"),
                                                        dmGameObject::PLAYBACK_ONCE_FORWARD, target, curve, 0.25f, 0, 0, 0, 0));
    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1u, state.m_Released);
    ASSERT_EQ(0u, m_FinishCount);
    m_UpdateContext.m_DT = 1.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(701u, state.m_Released);
    ASSERT_EQ(700u, m_FinishCount);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Objects); ++i)
    {
        ASSERT_EQ(4.0f, X(state.m_Objects[i]));
    }
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

class EulerRefreshAnimTest : public AnimTest
{
protected:
    void SetUp() override
    {
        AnimTest::SetUp();
        m_SetterCount = 0;
        m_PropertyObject = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, m_PropertyObject);
        for (uint32_t i = 0; i < 3; ++i)
        {
            m_Objects[i] = dmGameObject::New(m_Collection, 0);
            ASSERT_NE((dmGameObject::HInstance)0, m_Objects[i]);
        }
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(m_Objects[1], m_Objects[0]));
        dmGameObject::SetPosition(m_Objects[0], dmVMath::Point3(2, 3, 4));
        dmGameObject::SetPosition(m_Objects[1], dmVMath::Point3(1, -2, 3));
        dmGameObject::SetPosition(m_Objects[2], dmVMath::Point3(-1, 2, 5));
        dmGameObject::SetScale(m_Objects[0], dmVMath::Vector3(2, 3, 4));
        dmGameObject::UpdateTransforms(m_Collection);

        dmGameObject::Collection* collection = dmGameObject::GetCollectionFromHandle(m_Collection);
        m_Prototype = dmGameObject::GetPrototype(collection, dmGameObject::GetInstanceFromHandle(m_PropertyObject));
        m_SavedComponents = m_Prototype->m_Components;
        m_SavedComponentCount = m_Prototype->m_ComponentCount;
        m_Type.m_Context = this;
        m_Type.m_GetPropertyFunction = GetRefreshProperty;
        m_Type.m_SetPropertyFunction = SetRefreshProperty;
        // Attach a test component to the empty resource prototype. Restore it before instance destruction.
        m_Component = (dmGameObject::Prototype::Component*)malloc(sizeof(dmGameObject::Prototype::Component));
        *m_Component = dmGameObject::Prototype::Component(0, hash("refresh"), 0, &m_Type, 0,
            dmVMath::Point3(0), dmVMath::Quat::identity(), dmVMath::Vector3(1));
        m_Prototype->m_Components = m_Component;
        m_Prototype->m_ComponentCount = 1;
    }

    void TearDown() override
    {
        m_Prototype->m_Components = m_SavedComponents;
        m_Prototype->m_ComponentCount = m_SavedComponentCount;
        free(m_Component);
        AnimTest::TearDown();
    }

    static dmGameObject::PropertyResult CancelLaterAnimation(const dmGameObject::ComponentSetPropertyParams& params)
    {
        EulerRefreshAnimTest* test = (EulerRefreshAnimTest*)params.m_Context;
        ++test->m_SetterCount;
        return dmGameObject::CancelAnimations(test->m_Collection, test->m_Objects[1], 0, hash("position.x"));
    }

    static dmGameObject::PropertyResult GetRefreshProperty(const dmGameObject::ComponentGetPropertyParams& params,
                                                          dmGameObject::PropertyDesc& out_value)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(0.0f);
        out_value.m_ValuePtr = 0;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    static dmGameObject::PropertyResult SetRefreshProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        EulerRefreshAnimTest* test = (EulerRefreshAnimTest*)params.m_Context;
        if (test->m_RefreshCollection)
        {
            dmGameObject::UpdateTransforms(test->m_Collection);
        }
        else
        {
            dmGameObject::Collection* collection = dmGameObject::GetCollectionFromHandle(test->m_Collection);
            dmGameObject::UpdateTransformsForInstance(collection, dmGameObject::GetInstanceFromHandle(test->m_Objects[1]));
        }
        for (uint32_t i = 0; i < 3; ++i)
        {
            test->m_RefreshedRotations[i] = dmGameObject::GetRotation(test->m_Objects[i]);
            test->m_RefreshedMatrices[i] = dmGameObject::GetWorldMatrix(test->m_Objects[i]);
        }
        ++test->m_SetterCount;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    void TestRefresh(bool refresh_collection, bool animate_quaternion)
    {
        m_RefreshCollection = refresh_collection;
        m_UpdateContext.m_DT = 0.25f;
        dmVMath::Quat rotations[3];
        dmVMath::Matrix4 matrices[3];
        dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
        for (uint32_t i = 0; i < 3; ++i)
        {
            float angle = 90.0f * (i + 1);
            dmGameObject::PropertyVar target(angle);
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, m_Objects[i], 0, hash("euler.z"),
                dmGameObject::PLAYBACK_ONCE_FORWARD, target, easing, 1, 0, 0, 0, 0));
            rotations[i] = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, angle * 0.25f));
            matrices[i] = dmVMath::Matrix4::translation(dmVMath::Vector3(dmGameObject::GetPosition(m_Objects[i])))
                * dmVMath::Matrix4::rotation(rotations[i]) * dmVMath::Matrix4::scale(dmGameObject::GetScale(m_Objects[i]));
            if (i == 1)
                matrices[i] = matrices[0] * matrices[i];
        }
        dmVMath::Matrix4 unrelated_initial = dmGameObject::GetWorldMatrix(m_Objects[2]);
        dmGameObject::PropertyVar target(1.0f);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, m_PropertyObject, hash("refresh"), hash("value"),
            dmGameObject::PLAYBACK_ONCE_FORWARD, target, easing, 1, 0, 0, 0, 0));
        if (animate_quaternion)
        {
            // These pointer writes happen after the setter's refresh and must not consume final Euler writeback.
            dmGameObject::PropertyVar rotation_target(dmVMath::Quat::rotationX(1.0f));
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, m_Objects[1], 0, hash("rotation"),
                dmGameObject::PLAYBACK_ONCE_FORWARD, rotation_target, easing, 1, 0, 0, 0, 0));
        }

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_EQ(1u, m_SetterCount);
        for (uint32_t i = 0; i < 3; ++i)
        {
            bool refreshed = refresh_collection || i != 2;
            dmVMath::Quat refreshed_rotation = refreshed ? rotations[i] : dmVMath::Quat::identity();
            dmVMath::Matrix4 refreshed_matrix = refreshed ? matrices[i] : unrelated_initial;
            dmVMath::Quat final_rotation = dmGameObject::GetRotation(m_Objects[i]);
            const dmVMath::Matrix4& final_matrix = dmGameObject::GetWorldMatrix(m_Objects[i]);
            for (uint32_t axis = 0; axis < 4; ++axis)
            {
                ASSERT_NEAR(refreshed_rotation.getElem(axis), m_RefreshedRotations[i].getElem(axis), 0.0001f);
                ASSERT_NEAR(rotations[i].getElem(axis), final_rotation.getElem(axis), 0.0001f);
                for (uint32_t row = 0; row < 4; ++row)
                {
                    ASSERT_NEAR(refreshed_matrix.getElem(axis, row), m_RefreshedMatrices[i].getElem(axis, row), 0.0001f);
                    ASSERT_NEAR(matrices[i].getElem(axis, row), final_matrix.getElem(axis, row), 0.0001f);
                }
            }
        }
    }

    dmGameObject::HInstance m_PropertyObject;
    dmGameObject::HInstance m_Objects[3];
    dmGameObject::Prototype* m_Prototype;
    dmGameObject::Prototype::Component* m_SavedComponents;
    dmGameObject::Prototype::Component* m_Component;
    dmGameObject::ComponentType m_Type;
    uint32_t m_SavedComponentCount;
    uint32_t m_SetterCount;
    bool m_RefreshCollection;
    dmVMath::Quat m_RefreshedRotations[3];
    dmVMath::Matrix4 m_RefreshedMatrices[3];
};

// An earlier setter can cancel a ready lane; the later batch must not write that stopped animation.
TEST_F(EulerRefreshAnimTest, SetterCancelsLaterSimdLane)
{
    m_Type.m_SetPropertyFunction = CancelLaterAnimation;
    dmGameObject::PropertyVar target(8.0f);
    dmEasing::Curve curve(dmEasing::TYPE_LINEAR);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, m_PropertyObject, hash("refresh"), hash("value"),
                                                        dmGameObject::PLAYBACK_LOOP_FORWARD, target, curve, 1.0f, 0, 0, 0, 0));
    dmGameObject::HInstance objects[4] = { m_Objects[0], m_Objects[1], m_Objects[2], m_PropertyObject };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(objects); ++i)
    {
        dmGameObject::SetPosition(objects[i], dmVMath::Point3(0, 0, 0));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_FORWARD, target, curve, 1.0f, 0, AnimationStopped, this, 0));
    }
    m_UpdateContext.m_DT = 0.25f;
    for (uint32_t frame = 1; frame <= 2; ++frame)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_EQ(frame, m_SetterCount);
        ASSERT_EQ(0.0f, X(m_Objects[1]));
        ASSERT_EQ(2.0f * frame, X(m_Objects[0]));
        ASSERT_EQ(2.0f * frame, X(m_Objects[2]));
        ASSERT_EQ(2.0f * frame, X(m_PropertyObject));
        ASSERT_EQ(1u, m_CancelCount);
        ASSERT_EQ(0u, m_FinishCount);
    }
}

// A setter refresh must commit pending rotations on the requested ancestor chain, leaving other branches cached.
TEST_F(EulerRefreshAnimTest, EulerInstanceRefreshDuringEvaluation)
{
    TestRefresh(false, false);
}

// A collection refresh during evaluation must commit all pending Euler writes before calculating matrices.
TEST_F(EulerRefreshAnimTest, EulerCollectionRefreshDuringEvaluation)
{
    TestRefresh(true, false);
}

// Refreshing an ancestor chain must retain Euler precedence over quaternion tracks evaluated later in the pass.
TEST_F(EulerRefreshAnimTest, EulerInstanceRefreshBeforeQuaternionAnimation)
{
    TestRefresh(false, true);
}

// A collection refresh must retain final Euler writeback after subsequent quaternion animation pointer writes.
TEST_F(EulerRefreshAnimTest, EulerCollectionRefreshBeforeQuaternionAnimation)
{
    TestRefresh(true, true);
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

// More than 65k tracks must evaluate correctly, including cancellation and reuse of high indices.
TEST_F(AnimTest, AnimationPoolGrowthAndReuse)
{
    const uint32_t count = 100000;
    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, count + 1, 0);
    ASSERT_NE((dmGameObject::HCollection)0, m_Collection);

    dmArray<dmGameObject::HInstance> objects;
    objects.EnsureSize(count);
    dmGameObject::PropertyVar position_target(4.0f);
    dmGameObject::PropertyVar euler_target(180.0f);
    dmEasing::Curve easing(dmEasing::TYPE_LINEAR);
    m_UpdateContext.m_DT = 0.25f;
    for (uint32_t i = 0; i < count; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, objects[i]);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
            dmGameObject::PLAYBACK_ONCE_FORWARD, position_target, easing, 1, 0, AnimationStopped, this, 0));
        if (i % 20 < 3)
        {
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("euler.z"),
                dmGameObject::PLAYBACK_ONCE_FORWARD, euler_target, easing, 1, 0, 0, 0, 0));
        }
    }
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmVMath::Quat expected_rotation = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, 45));
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_NEAR(1.0f, X(objects[i]), EPSILON);
        if (i % 20 < 3)
        {
            for (uint32_t axis = 0; axis < 4; ++axis)
                ASSERT_NEAR(expected_rotation.getElem(axis), dmGameObject::GetRotation(objects[i]).getElem(axis), 0.0001f);
        }
    }
    dmGameObject::CancelAnimationCallbacks(m_Collection, this);
    for (uint32_t i = 0; i < count; i += 2)
    {
        dmGameObject::CancelAnimations(m_Collection, objects[i]);
        dmGameObject::Delete(m_Collection, objects[i], false);
    }
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PropertyVar replacement_target(8.0f);
    for (uint32_t i = 0; i < count; i += 2)
    {
        dmGameObject::HInstance previous = objects[i];
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, objects[i]);
        ASSERT_NE(previous, objects[i]);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
            dmGameObject::PLAYBACK_ONCE_FORWARD, replacement_target, easing, 1, 0, AnimationStopped, this, 0));
    }
    for (uint32_t frame = 0; frame < 4; ++frame)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_NEAR((i & 1) ? 4.0f : 8.0f, X(objects[i]), EPSILON);
    ASSERT_EQ(count / 2, m_FinishCount);
    ASSERT_EQ(0u, m_CancelCount);
}

struct AnimationGrowthCallbackState
{
    AnimTest* m_Test;
    dmArray<dmGameObject::HInstance> m_Objects;
    bool m_Called;
};

static void CountGrowthAnimationStopped(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
    bool finished, void* userdata1, void* userdata2)
{
    ASSERT_TRUE(finished);
    ++((AnimTest*)userdata2)->m_FinishCount;
}

static void GrowAnimationsCallback(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
    bool finished, void* userdata1, void* userdata2)
{
    AnimationGrowthCallbackState* state = (AnimationGrowthCallbackState*)userdata1;
    ASSERT_TRUE(finished);
    state->m_Called = true;
    dmGameObject::PropertyVar target(4.0f);
    for (uint32_t i = 0; i < state->m_Objects.Size(); ++i)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(state->m_Test->m_Collection, state->m_Objects[i], 0,
            hash("position.x"), dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR),
            1, 0, CountGrowthAnimationStopped, &state->m_Objects[i], state->m_Test));
    }
}

// Completion callbacks may grow every animation container beyond 65k, including the listener table.
TEST_F(AnimTest, AnimationPoolGrowthDuringCallback)
{
    const uint32_t count = 70000;
    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, count + 1, 0);
    ASSERT_NE((dmGameObject::HCollection)0, m_Collection);

    AnimationGrowthCallbackState state;
    state.m_Test = this;
    state.m_Called = false;
    state.m_Objects.EnsureSize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        state.m_Objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_NE((dmGameObject::HInstance)0, state.m_Objects[i]);
    }
    dmGameObject::PropertyVar target(1.0f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, state.m_Objects[0], 0, hash("position.y"),
        dmGameObject::PLAYBACK_ONCE_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR),
        0.25f, 0, GrowAnimationsCallback, &state, 0));
    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(state.m_Called);
    ASSERT_EQ(0u, m_FinishCount);
    m_UpdateContext.m_DT = 1.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_NEAR(4.0f, X(state.m_Objects[i]), EPSILON);
    ASSERT_EQ(count, m_FinishCount);
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

// Adding and deleting a component property track must preserve the values of
// built-in and custom looping tracks.
TEST_F(AnimTest, ComponentPropertyTracksPreserveLoopValues)
{
    dmGameObject::HInstance objects[6];
    dmGameObject::HInstance scripted = 0;
    dmVMath::FloatVector samples(2);
    samples.values[0] = 0;
    samples.values[1] = 1;
    dmEasing::Curve custom(dmEasing::TYPE_FLOAT_VECTOR);
    custom.vector = &samples;
    dmGameObject::PropertyVar target(16.0f);
    for (uint32_t i = 0; i < 6; ++i)
    {
        objects[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.z"),
                                                            dmGameObject::PLAYBACK_LOOP_FORWARD, target, custom, 1, 0, 0, 0, 0));
    }
    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, objects[i], 0, hash("position.x"),
                                                            dmGameObject::PLAYBACK_LOOP_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 1, 0, 0, 0, 0));
    }
    m_UpdateContext.m_DT = 0.0625f;
    for (uint32_t frame = 0; frame < 8; ++frame)
    {
        if (frame == 2)
        {
            scripted = Spawn(m_Factory, m_Collection, "/demo_box.goc", hash("ordered-component"), 0,
                             dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
            ASSERT_NE((dmGameObject::HInstance)0, scripted);
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, Animate(m_Collection, scripted, hash("script"), hash("tint.x"),
                                                                dmGameObject::PLAYBACK_LOOP_FORWARD, target, dmEasing::Curve(dmEasing::TYPE_LINEAR), 1, 0, 0, 0, 0));
        }
        if (frame == 5)
        {
            dmGameObject::Delete(m_Collection, scripted, false);
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        for (uint32_t i = 0; i < 6; ++i)
        {
            ASSERT_NEAR((float)(frame + 1), X(objects[i]), 0.00001f);
        }
    }
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
