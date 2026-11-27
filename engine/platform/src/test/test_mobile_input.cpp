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

#include "../mobile_shared/window_mobile_private.h"

// The null-backend test executable supplies the single mobile window used by window_input.c.
Nativewin dmNativeWin;

class MobileInput : public jc_test_base_class
{
    protected:
    MobileInputCallbacks m_Callbacks;

    void SetUp()
    {
        memset(&dmNativeWin, 0, sizeof(dmNativeWin));
        memset(&m_Callbacks, 0, sizeof(m_Callbacks));
        dmNativeWin.inputCallbacks = &m_Callbacks;
        dmNativeClearInput();
        dmNativeWin.opened = 1;
    }

};

// Discrete key presses survive repeated events and reject unmapped keys and Android actions.
TEST_F(MobileInput, KeyEvents)
{
    dmNativeInputKey(-1, NATIVE_PRESS);
    dmNativeInputKey(NATIVE_KEY_LAST + 1, NATIVE_PRESS);
    ASSERT_EQ(0, dmNativeGetKey(-1));
    ASSERT_EQ(0, dmNativeGetKey(NATIVE_KEY_LAST + 1));
    dmNativeInputKey(NATIVE_KEY_BACK, NATIVE_PRESS);
    dmNativeInputKey(NATIVE_KEY_BACK, NATIVE_PRESS);
    dmNativeInputKey(NATIVE_KEY_BACK, -1);
    ASSERT_EQ(1, dmNativeGetKey(NATIVE_KEY_BACK));
    dmNativeInputKey(NATIVE_KEY_BACK, NATIVE_RELEASE);
    ASSERT_EQ(0, dmNativeGetKey(NATIVE_KEY_BACK));
}

// A tap between polls is reported once and cannot discard a subsequent held press.
TEST_F(MobileInput, TapBetweenPolls)
{
    dmNativeInput.MousePositionFromTouch = 1;
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_LEFT, NATIVE_PRESS);
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_LEFT, NATIVE_RELEASE);
    ASSERT_EQ(1, dmNativeInput.MouseLeftButtonFromTouch);
    ASSERT_EQ(0, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_RIGHT));
    ASSERT_EQ(1, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_LEFT));
    ASSERT_EQ(0, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_LEFT));
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_RIGHT, NATIVE_PRESS);
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_RIGHT, NATIVE_RELEASE);
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_RIGHT, NATIVE_PRESS);
    ASSERT_EQ(1, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_RIGHT));
    ASSERT_EQ(1, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_RIGHT));
}

static int g_TextCount;
static void OnCharacter(void* user_data, int codepoint)
{
    ++g_TextCount;
    *(int*)user_data = codepoint;
}

// Text input filters control characters and forwards a Unicode press only to a registered callback.
TEST_F(MobileInput, TextEvents)
{
    g_TextCount = 0;
    int codepoint = 0;
    m_Callbacks.m_CharCallback = OnCharacter;
    m_Callbacks.m_CharUserData = &codepoint;
    dmNativeInputChar(0x1f, NATIVE_PRESS);
    dmNativeInputChar(0x7f, NATIVE_PRESS);
    dmNativeInputChar(0x9f, NATIVE_PRESS);
    dmNativeInputChar('a', NATIVE_RELEASE);
    ASSERT_EQ(0, g_TextCount);
    dmNativeInputChar(' ', NATIVE_PRESS);
    dmNativeInputChar(0x1f600, NATIVE_PRESS);
    ASSERT_EQ(2, g_TextCount);
    ASSERT_EQ(0x1f600, codepoint);
    m_Callbacks.m_CharCallback = 0;
    dmNativeInputChar('a', NATIVE_PRESS);
    ASSERT_EQ(2, g_TextCount);
}

// Opening the next window starts with clean pointer, keyboard and touch state.
TEST_F(MobileInput, Reset)
{
    dmNativeInput.Touch[0].Reference = &dmNativeInput;
    dmNativeInputKey('A', NATIVE_PRESS);
    dmNativeInputMouseClick(NATIVE_MOUSE_BUTTON_LAST, NATIVE_PRESS);
    dmNativeWin.opened = 0;
    ASSERT_EQ(0, dmNativeGetKey('A'));
    ASSERT_EQ(0, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_LAST));
    dmNativeClearInput();
    dmNativeWin.opened = 1;
    ASSERT_EQ(0, dmNativeGetKey('A'));
    ASSERT_EQ(0, dmNativeGetMouseButton(NATIVE_MOUSE_BUTTON_LAST));
    WindowTouchData touches[NATIVE_MAX_TOUCH];
    ASSERT_EQ(0u, dmNativeReadTouches(dmNativeInput.Touch, touches, NATIVE_MAX_TOUCH, 1));
    ASSERT_EQ(NATIVE_PHASE_IDLE, dmNativeInput.Touch[NATIVE_MAX_TOUCH - 1].Phase);
    ASSERT_EQ(NATIVE_MAX_TOUCH - 1, dmNativeInput.Touch[NATIVE_MAX_TOUCH - 1].Id);
}
