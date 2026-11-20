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

#include "internal.h"

NativeInput dmNativeInput;

void dmNativeClearInput(void)
{
    memset(&dmNativeInput, 0, sizeof(dmNativeInput));
    dmNativeResetTouches(dmNativeInput.Touch);
}

void dmNativeInputKey(int key, int action)
{
    if ((unsigned)key < sizeof(dmNativeInput.keys) && (action == NATIVE_PRESS || action == NATIVE_RELEASE))
    {
        dmNativeInput.keys[key] = (uint8_t)action;
    }
}

int dmNativeGetKey(int key)
{
    return dmNativeWin.opened && (unsigned)key < sizeof(dmNativeInput.keys) && dmNativeInput.keys[key];
}

void dmNativeInputMouseClick(int button, int action)
{
    if ((unsigned)button > NATIVE_MOUSE_BUTTON_LAST)
    {
        return;
    }

    unsigned mask = 1u << button;
    if (action == NATIVE_PRESS)
    {
        dmNativeInput.heldButtons |= mask;
        if (button == NATIVE_MOUSE_BUTTON_LEFT)
        {
            dmNativeInput.MouseLeftButtonFromTouch = dmNativeInput.MousePositionFromTouch;
        }
    }
    else if (action == NATIVE_RELEASE && (dmNativeInput.heldButtons & mask))
    {
        // Keep a tap visible until the next input poll, even if both events arrived this frame.
        dmNativeInput.releasedButtons |= mask;
        dmNativeInput.heldButtons &= ~mask;
    }
}

int dmNativeGetMouseButton(int button)
{
    if (!dmNativeWin.opened || (unsigned)button > NATIVE_MOUSE_BUTTON_LAST)
    {
        return 0;
    }

    unsigned mask = 1u << button;
    unsigned pressed = dmNativeInput.heldButtons | dmNativeInput.releasedButtons;
    dmNativeInput.releasedButtons &= ~mask;
    return (pressed & mask) != 0;
}

int dmNativeIsMouseLeftButtonFromTouch(void)
{
    return dmNativeInput.MouseLeftButtonFromTouch;
}

int dmNativeIsMousePositionFromTouch(void)
{
    return dmNativeInput.MousePositionFromTouch;
}

void dmNativeGetMousePos(int* x, int* y)
{
    if (dmNativeWin.opened)
    {
        if (x)
        {
            *x = dmNativeInput.MousePosX;
        }

        if (y)
        {
            *y = dmNativeInput.MousePosY;
        }
    }
}

int dmNativeGetMouseWheel(void)
{
    return 0;
}

void dmNativeSetCursorVisible(int visible)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.cursorHidden = !visible;
    }
}

int dmNativeGetMouseLocked(void)
{
    return dmNativeWin.cursorHidden;
}

int dmNativeSetCharCallback(Nativecharfun callback)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.charCallback = callback;
    }

    return dmNativeWin.opened;
}

void dmNativeInputChar(int character, int action)
{
    // Platform text events exclude C0, DEL and C1 control characters.
    if (character < 0x20 || (character >= 0x7f && character < 0xa0))
    {
        return;
    }

    if (action == NATIVE_PRESS && dmNativeWin.charCallback)
    {
        dmNativeWin.charCallback(character, action);
    }
}

int dmNativeSetMarkedTextCallback(Nativemarkedtextfun callback)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.markedTextCallback = callback;
    }

    return dmNativeWin.opened;
}

void dmNativeSetMarkedText(char* text)
{
    if (dmNativeWin.markedTextCallback)
    {
        dmNativeWin.markedTextCallback(text);
    }
}

int dmNativeGetTouch(NativeTouch* output, int capacity, int* count)
{
    *count = dmNativeReadTouches(dmNativeInput.Touch, output, capacity, 1);
    return 1;
}
