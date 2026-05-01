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

#pragma once

#include <jc_test/jc_test.h>

#include <resource/resource.h>

#include <hid/hid.h>

#include <sound/sound.h>
#include <gameobject/gameobject.h>
#include <gamesys/gamesys.h>
#include "../engine.h"
#include "../engine_private.h"

class EngineTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_DT = 1.0f / 60.0f;
    }

    void TearDown() override
    {
    }

    float m_DT;
};


template<typename T>
class EngineParamsTest : public jc_test_params_class<T>
{
protected:
    void SetUp() override
    {
        m_DT = 1.0f / 60.0f;
        dmEngineInitialize();
        m_Engine = dmEngine::New(0);
    }

    void TearDown() override
    {
        dmEngine::Delete(m_Engine);
        dmEngineFinalize();
    }

    dmEngine::HEngine m_Engine;
    float m_DT;
};

struct DrawCountParams
{
    const char* m_ProjectPath;
    uint32_t m_NumSkipFrames;   // Number of frames to skip before the actual test is done
    uint64_t m_ExpectedDrawCount;
};

class DrawCountTest : public EngineParamsTest<DrawCountParams>
{
public:
    virtual ~DrawCountTest() {}
};
