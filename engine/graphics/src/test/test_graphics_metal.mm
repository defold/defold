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

#include <QuartzCore/QuartzCore.h>

#include "metal/graphics_metal_private.h"

class MetalArgumentBufferTest : public jc_test_base_class
{
public:
    NS::AutoreleasePool*                m_AutoreleasePool;
    MTL::Device*                        m_Device;
    MTL::ArgumentEncoder*               m_Encoder;
    dmGraphics::MetalArgumentBufferPool m_Pool;

    void SetUp() override
    {
        m_AutoreleasePool = NS::AutoreleasePool::alloc()->init();
        m_Device = MTL::CreateSystemDefaultDevice();
        ASSERT_NE((MTL::Device*)0, m_Device);

        MTL::ArgumentDescriptor* argument = MTL::ArgumentDescriptor::argumentDescriptor();
        argument->setDataType(MTL::DataTypePointer);
        argument->setIndex(0);
        m_Encoder = m_Device->newArgumentEncoder(NS::Array::array(argument));
        ASSERT_NE((MTL::ArgumentEncoder*)0, m_Encoder);

        // Preallocate two buffers so the pool can be tested without a graphics context.
        m_Pool.m_ScratchBufferIndex = 0;
        m_Pool.m_SizePerBuffer = 512;
        m_Pool.m_ScratchBufferPool.SetCapacity(2);
        for (uint32_t i = 0; i < 2; ++i)
        {
            dmGraphics::MetalConstantScratchBuffer buffer = {};
            buffer.m_DeviceBuffer.m_Base.m_Size = m_Pool.m_SizePerBuffer;
            buffer.m_DeviceBuffer.m_Buffer = m_Device->newBuffer(m_Pool.m_SizePerBuffer, MTL::ResourceStorageModeShared);
            m_Pool.m_ScratchBufferPool.Push(buffer);
        }
    }

    void TearDown() override
    {
        m_Encoder->release();
        for (uint32_t i = 0; i < m_Pool.m_ScratchBufferPool.Size(); ++i)
        {
            m_Pool.m_ScratchBufferPool[i].m_DeviceBuffer.m_Buffer->release();
        }
        m_Device->release();
        m_AutoreleasePool->release();
    }
};

TEST_F(MetalArgumentBufferTest, ConsecutiveBindingsAreAligned)
{
    dmGraphics::MetalArgumentBinding first = m_Pool.Bind(0, m_Encoder);
    dmGraphics::MetalArgumentBinding second = m_Pool.Bind(0, m_Encoder);

    ASSERT_EQ(0u, first.m_Offset);
    ASSERT_EQ(first.m_Buffer, second.m_Buffer);
    ASSERT_GE(second.m_Offset, first.m_Offset + m_Encoder->encodedLength());
    ASSERT_EQ(0u, second.m_Offset % m_Encoder->alignment());
#if defined(IOS_SIMULATOR)
    ASSERT_EQ(0u, second.m_Offset % 256);
#endif
    ASSERT_LE(second.m_Offset + m_Encoder->encodedLength(), m_Pool.m_SizePerBuffer);
}

TEST_F(MetalArgumentBufferTest, AllocationAlignsCursor)
{
    m_Pool.Get()->Advance(8);
    dmGraphics::MetalConstantScratchBuffer* buffer = m_Pool.Allocate(0, 8, 256);

    ASSERT_EQ(0u, m_Pool.m_ScratchBufferIndex);
    ASSERT_EQ(256u, buffer->m_MappedDataCursor);
}

TEST_F(MetalArgumentBufferTest, PaddingRolloverAndRewind)
{
    dmGraphics::MetalConstantScratchBuffer* first = m_Pool.Get();
    first->Advance(m_Pool.m_SizePerBuffer - 8);
    dmGraphics::MetalConstantScratchBuffer* second = m_Pool.Allocate(0, 8, 256);

    ASSERT_EQ(1u, m_Pool.m_ScratchBufferIndex);
    ASSERT_EQ(0u, second->m_MappedDataCursor);
    ASSERT_EQ(m_Pool.m_SizePerBuffer - 8, first->m_MappedDataCursor);

    dmGraphics::MetalArgumentBinding binding = m_Pool.Bind(0, m_Encoder);
    ASSERT_EQ(second->m_DeviceBuffer.m_Buffer, binding.m_Buffer);
    ASSERT_EQ(0u, binding.m_Offset);

    m_Pool.Rewind();
    binding = m_Pool.Bind(0, m_Encoder);
    ASSERT_EQ(0u, m_Pool.m_ScratchBufferIndex);
    ASSERT_EQ(first->m_DeviceBuffer.m_Buffer, binding.m_Buffer);
    ASSERT_EQ(0u, binding.m_Offset);
    ASSERT_EQ(0u, second->m_MappedDataCursor);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
