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

// Exercise private allocation and decoder layout without adding a runtime test API.
#include "../sound.cpp"

// Verifies 512-bit alignment and preserved contents when audio buffers grow or shrink.
TEST(SoundBuffers, AllocationAndGrowth)
{
    const uint32_t sizes[] = { 1, 17, 31, 63, 64, 65, 257, 19 };
    uint8_t* buffer = 0;
    uint32_t previous_size = 0;
    for (uint32_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
    {
        uint8_t* resized = (uint8_t*)dmSound::ReallocBuffer(buffer, previous_size, sizes[i]);
        ASSERT_NE((uint8_t*)0, resized);
        buffer = resized;
        ASSERT_EQ((uintptr_t)0, (uintptr_t)buffer % 64);
        for (uint32_t j = 0; j < dmMath::Min(previous_size, sizes[i]); ++j)
        {
            ASSERT_EQ((uint8_t)(j * 37), buffer[j]);
        }

        for (uint32_t j = 0; j < sizes[i]; ++j)
        {
            buffer[j] = (uint8_t)(j * 37);
        }

        previous_size = sizes[i];
    }

    dmMemory::AlignedFree(buffer);
}

// Verifies that decoder history does not misalign sample zero or move when buffers grow.
TEST(SoundBuffers, DecoderHistoryAndGrowth)
{
    dmSound::SoundSystem sound;
    sound.m_DecoderBufferFrameCapacity = 0;
    sound.m_DecoderTempOutputCapacity = 0;
    sound.m_DecoderTempOutput = 0;
    for (uint32_t c = 0; c < SOUND_MAX_DECODE_CHANNELS; ++c)
    {
        sound.m_DecoderOutput[c] = 0;
    }

    dmSoundCodec::Info info;
    memset(&info, 0, sizeof(info));
    info.m_Channels = 2;
    info.m_BitsPerSample = 16;
    info.m_IsInterleaved = 1;

    const uint32_t capacities[] = { 33, 129, 1025 };
    uint32_t previous_capacity = 0;
    for (uint32_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i)
    {
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::EnsureDecoderScratchBufferSize(&sound, info, capacities[i]));
        ASSERT_EQ((uintptr_t)0, (uintptr_t)sound.m_DecoderTempOutput % 64);
        ASSERT_EQ(capacities[i], sound.m_DecoderBufferFrameCapacity);
        for (uint32_t c = 0; c < SOUND_MAX_DECODE_CHANNELS; ++c)
        {
            float* samples = sound.GetDecoderBufferBase(c);
            ASSERT_EQ((uintptr_t)0, (uintptr_t)samples % 64);
            for (uint32_t j = 0; j < previous_capacity; ++j)
            {
                ASSERT_EQ((float)(j + c * 1024), samples[(int32_t)j - SOUND_MAX_HISTORY]);
            }

            // Include the entire history and the last available future sample.
            for (uint32_t j = 0; j < capacities[i]; ++j)
            {
                samples[(int32_t)j - SOUND_MAX_HISTORY] = (float)(j + c * 1024);
            }
        }

        previous_capacity = capacities[i];
    }

    // Prefix padding must be included in the overflow check before allocating.
    ASSERT_EQ(dmSound::RESULT_OUT_OF_MEMORY, dmSound::EnsureDecoderScratchBufferSize(&sound, info, 0xffffffffU / sizeof(float)));
    ASSERT_EQ(previous_capacity, sound.m_DecoderBufferFrameCapacity);
    for (uint32_t c = 0; c < SOUND_MAX_DECODE_CHANNELS; ++c)
    {
        dmMemory::AlignedFree(sound.m_DecoderOutput[c]);
    }

    dmMemory::AlignedFree(sound.m_DecoderTempOutput);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
