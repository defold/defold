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

#include <dlib/thread.h>

#include "../box2d/box2d_physics.h"

// A separate executable keeps earlier worlds from hiding timer initialization races under TSan.
TEST(Box2DThreadingStartup, FirstWorldUsesMultipleWorkers)
{
    const char* argv[] = { "test_physics_threading_startup", "--config=box2d.worker_count=3" };
    dmConfigFile::HConfig config = 0;
    ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer(0, 0, 2, argv, &config));
    dmPhysics::NewContextParams params;
    params.m_Config = config;
    dmPhysics::HContext2D context = dmPhysics::NewContext2D(params);
    dmConfigFile::Delete(config);
    ASSERT_NE((void*)0, context);

    dmPhysics::HWorld2D world = dmPhysics::NewWorld2D(context, dmPhysics::NewWorldParams());
    ASSERT_NE((void*)0, world);
    int expected_workers = 1;
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    if (dmThread::PlatformHasThreadSupport())
    {
        expected_workers = 4;
    }
#endif
    ASSERT_EQ(expected_workers, b2World_GetWorkerCount(world->m_WorldId));

    dmPhysics::StepWorldContext step;
    step.m_DT = 1.0f / 30.0f;
    step.m_Box2DSubStepCount = 4;
    dmPhysics::StepWorld2D(world, step);

    dmPhysics::DeleteWorld2D(context, world);
    dmPhysics::DeleteContext2D(context);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
