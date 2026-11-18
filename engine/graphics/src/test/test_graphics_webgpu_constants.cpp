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

// Exercise the production setter without a GPU or an engine API test hook.
#include "../webgpu/graphics_webgpu.cpp"

struct WebGPUConstantsTest : jc_test_base_class
{
    dmGraphics::WebGPUContext*   m_Context;
    dmGraphics::WebGPUProgram    m_Program;
    dmGraphics::HUniformLocation m_Location;
    dmVMath::Vector4             m_Data[6];

    void SetUp()
    {
        m_Context = new dmGraphics::WebGPUContext();
        m_Context->m_CurrentProgram = &m_Program;
        m_Program.m_UniformData = (uint8_t*)m_Data;
        // Include both a resource offset and a member offset in a nonzero set/binding.
        m_Program.m_BaseProgram.m_ResourceBindings[1][2].m_UniformBufferOffset = 2 * sizeof(dmVMath::Vector4);
        m_Location = 1 | ((uint64_t)2 << 16) | ((uint64_t)sizeof(dmVMath::Vector4) << 32);
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Data); ++i)
        {
            m_Data[i] = dmVMath::Vector4((float)i, 1.0f, 2.0f, 3.0f);
        }

        for (uint32_t set = 0; set < dmGraphics::MAX_SET_COUNT; ++set)
        {
            m_Program.m_BindGroupIndices[set] = 10 + set;
        }
    }

    void TearDown()
    {
        delete m_Context;
    }

};

// Identical vec4 arrays must retain cached bind groups, avoiding redundant uniform uploads.
TEST_F(WebGPUConstantsTest, IdenticalValuesPreserveBindGroups)
{
    dmVMath::Vector4 values[] = { m_Data[3], m_Data[4] };
    dmVMath::Vector4 expected[DM_ARRAY_SIZE(m_Data)];
    memcpy(expected, m_Data, sizeof(expected));

    WebGPUSetConstantV4((dmGraphics::HContext)m_Context, values, DM_ARRAY_SIZE(values), m_Location);

    ASSERT_EQ(0, memcmp(expected, m_Data, sizeof(expected)));
    for (uint32_t set = 0; set < dmGraphics::MAX_SET_COUNT; ++set)
    {
        ASSERT_EQ(10 + set, m_Program.m_BindGroupIndices[set]);
    }
}

// A change in the second vec4 must update the full range and invalidate only its set.
TEST_F(WebGPUConstantsTest, ChangedValuesInvalidateOnlyTheirBindGroup)
{
    dmVMath::Vector4 values[] = { m_Data[3], dmVMath::Vector4(4.0f, 5.0f, 6.0f, 7.0f) };
    dmVMath::Vector4 expected[DM_ARRAY_SIZE(m_Data)];
    memcpy(expected, m_Data, sizeof(expected));
    expected[4] = values[1];

    WebGPUSetConstantV4((dmGraphics::HContext)m_Context, values, DM_ARRAY_SIZE(values), m_Location);

    ASSERT_EQ(0, memcmp(expected, m_Data, sizeof(expected)));
    for (uint32_t set = 0; set < dmGraphics::MAX_SET_COUNT; ++set)
    {
        uint32_t expected_index = set == 1 ? dmGraphics::INVALID_BIND_GROUP_INDEX : 10 + set;
        ASSERT_EQ(expected_index, m_Program.m_BindGroupIndices[set]);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
