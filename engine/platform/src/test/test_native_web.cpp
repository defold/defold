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
#include <emscripten.h>

#include "../window.hpp"
#include "../window_touch.h"

// Native web lookup must reach the linked Emscripten resolver.
TEST(NativeWeb, GetProcAddress)
{
    ASSERT_NE((uintptr_t)0, dmPlatform::GetProcAddress(0, "glClear"));
}

// Extension aliases must resolve to the same address as their core functions.
TEST(NativeWeb, GetProcAddressExtensionAlias)
{
    uintptr_t address = dmPlatform::GetProcAddress(0, "glActiveTexture");
    ASSERT_NE((uintptr_t)0, address);
    ASSERT_EQ(address, dmPlatform::GetProcAddress(0, "glActiveTextureARB"));
}

// Unsupported function names must return null so callers can detect unavailable functionality.
TEST(NativeWeb, GetProcAddressUnknownFunction)
{
    ASSERT_EQ((uintptr_t)0, dmPlatform::GetProcAddress(0, "glNonexistentFunction"));
}

class NativeWebTouchTest : public jc_test_base_class
{
    protected:
    HWindow m_Window;

    void SetUp() override
    {
        m_Window = dmPlatform::NewWindow();
        ASSERT_NE((HWindow)0, m_Window);
    }

    void TearDown() override
    {
        dmPlatform::DeleteWindow(m_Window);
    }

    static void SendTouch(int id, int x, int y, int phase)
    {
        EM_ASM({ DefoldPlatform.fillTouch($0, $1, $2, $3); }, id, x, y, phase);
    }

};

// Verifies duplicate updates for both fingers preserve movement until read, guarding against Firefox's duplicate touchmove events (#10264).
TEST_F(NativeWebTouchTest, DuplicateMovesPreserveDeltas)
{
    WindowTouchData touches[NATIVE_MAX_TOUCH];

    SendTouch(10, 100, 100, NATIVE_PHASE_BEGAN);
    SendTouch(20, 200, 200, NATIVE_PHASE_BEGAN);
    ASSERT_EQ(2, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));

    for (int event = 0; event < 2; ++event)
    {
        SendTouch(10, 110, 105, NATIVE_PHASE_MOVED);
        SendTouch(20, 195, 212, NATIVE_PHASE_MOVED);
    }

    ASSERT_EQ(2, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    ASSERT_EQ(10, touches[0].m_Id);
    ASSERT_EQ(110, touches[0].m_X);
    ASSERT_EQ(105, touches[0].m_Y);
    ASSERT_EQ(10, touches[0].m_DX);
    ASSERT_EQ(5, touches[0].m_DY);
    ASSERT_EQ(20, touches[1].m_Id);
    ASSERT_EQ(195, touches[1].m_X);
    ASSERT_EQ(212, touches[1].m_Y);
    ASSERT_EQ(-5, touches[1].m_DX);
    ASSERT_EQ(12, touches[1].m_DY);

    ASSERT_EQ(2, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    for (int i = 0; i < 2; ++i)
    {
        ASSERT_EQ(0, touches[i].m_DX);
        ASSERT_EQ(0, touches[i].m_DY);
    }
}

// Verifies multiple moves report net displacement since the last read, including a return to the starting position.
TEST_F(NativeWebTouchTest, MultipleMovesAccumulateDeltas)
{
    WindowTouchData touches[NATIVE_MAX_TOUCH];

    SendTouch(10, 100, 100, NATIVE_PHASE_BEGAN);
    ASSERT_EQ(1, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));

    SendTouch(10, 104, 98, NATIVE_PHASE_MOVED);
    SendTouch(10, 110, 95, NATIVE_PHASE_MOVED);
    ASSERT_EQ(1, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    ASSERT_EQ(110, touches[0].m_X);
    ASSERT_EQ(95, touches[0].m_Y);
    ASSERT_EQ(10, touches[0].m_DX);
    ASSERT_EQ(-5, touches[0].m_DY);

    SendTouch(10, 115, 90, NATIVE_PHASE_MOVED);
    SendTouch(10, 110, 95, NATIVE_PHASE_MOVED);
    ASSERT_EQ(1, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    ASSERT_EQ(0, touches[0].m_DX);
    ASSERT_EQ(0, touches[0].m_DY);
}

// Verifies a press, move and release before one read preserve movement once without repeating it on the synthetic release.
TEST_F(NativeWebTouchTest, TapPreservesMovement)
{
    WindowTouchData touches[NATIVE_MAX_TOUCH];

    SendTouch(10, 100, 100, NATIVE_PHASE_BEGAN);
    SendTouch(10, 110, 105, NATIVE_PHASE_MOVED);
    SendTouch(10, 110, 105, NATIVE_PHASE_ENDED);
    ASSERT_EQ(1, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    ASSERT_EQ(NATIVE_PHASE_BEGAN, touches[0].m_Phase);
    ASSERT_EQ(10, touches[0].m_DX);
    ASSERT_EQ(5, touches[0].m_DY);

    ASSERT_EQ(1, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
    ASSERT_EQ(NATIVE_PHASE_ENDED, touches[0].m_Phase);
    ASSERT_EQ(0, touches[0].m_DX);
    ASSERT_EQ(0, touches[0].m_DY);
    ASSERT_EQ(0, dmPlatform::GetTouchData(m_Window, touches, NATIVE_MAX_TOUCH));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
