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

#ifndef DM_TEST_WINDOW_FOCUS_H
#define DM_TEST_WINDOW_FOCUS_H

#include "../window.hpp"

namespace dmWindowTest
{
    struct FocusLossInput
    {
        HWindow m_Window;
        int     m_Losses, m_Key, m_Button;

        static void Record(void* data, uint32_t focused)
        {
            FocusLossInput* observed = (FocusLossInput*)data;
            if (!focused)
            {
                ++observed->m_Losses;
                observed->m_Key = dmPlatform::GetKey(observed->m_Window, 'A');
                observed->m_Button = dmPlatform::GetMouseButton(observed->m_Window, 0);
            }
        }

    };
}

#endif
