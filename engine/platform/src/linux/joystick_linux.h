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

#ifndef DM_JOYSTICK_LINUX_H
#define DM_JOYSTICK_LINUX_H

#include <linux/input.h>
#include <stdint.h>

namespace dmPlatform
{
    struct EvdevPacket
    {
        int16_t       m_ButtonsByCode[KEY_CNT - BTN_MISC], m_AxesByCode[ABS_CNT];
        input_absinfo m_Ranges[ABS_CNT];
        float         m_Axes[ABS_CNT];
        uint8_t       m_Buttons[KEY_CNT - BTN_MISC + 16], m_Hats[4];
        uint16_t      m_ButtonCount, m_AxisCount, m_HatCount;
    };
    void InitEvdevPacket(EvdevPacket* packet, const uint8_t* key_bits, const uint8_t* abs_bits);
    void ApplyEvdevEvent(EvdevPacket* packet, unsigned type, unsigned code, int value);
    void EvdevGuid(const input_id& id, const char* name, char guid[33]);
}
#endif
