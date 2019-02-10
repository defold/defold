#pragma once

#include <jc/test.h>

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
    virtual void SetUp()
    {
        m_DT = 1.0f / 60.0f;
    }

    virtual void TearDown()
    {
    }

    float m_DT;
};


template<typename T>
class EngineParamsTest : public jc_test_params_class<T>
{
protected:
    virtual void SetUp()
    {
        m_DT = 1.0f / 60.0f;
        m_Engine = dmEngine::New(0);
    }

    virtual void TearDown()
    {
        dmEngine::Delete(m_Engine);
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
