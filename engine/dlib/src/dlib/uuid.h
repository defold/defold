// Copyright 2020 The Defold Foundation
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

#ifndef DM_UUID
#define DM_UUID

namespace dmUUID
{
    union UUID
    {
        uint8_t m_UUID[16];
        struct
        {
            uint32_t m_TimeLow;
            uint16_t m_TimeMid;
            uint16_t m_TimeHiAndVersion;
            uint16_t m_ClockSeq;
            uint8_t  m_Node[6];
        };
    };

    /**
     * Generate UUID
     * @param uuid pointer to UUID struct
     */
    void Generate(UUID* uuid);
}

#endif // DM_UUID
