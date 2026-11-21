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

// Muting must preserve the cursor, retained FIR samples and audible transitions for instance and group mutes.
TEST(SoundBuffers, MutedResamplingPreservesState)
{
    dmSound::SoundSystem sound;
    dmSound::g_SoundSystem = &sound;
    dmSound::SelectDSPImpl(&sound.m_DSP, dmSound::DSPIMPL_TYPE_DEFAULT);
    DM_ALIGNED(64) float decoder[2][512];
    DM_ALIGNED(64) float output[2][2][65];
    float state[2][2][32];
    sound.m_DecoderOutput[0] = decoder[0];
    sound.m_DecoderOutput[1] = decoder[1];
    const uint64_t deltas[] = { 1, 511, 1024, 1882, 2048, 2229, 3073, 8193 };
    const uint32_t phases[] = { 0, 1, 1023, 2047 };
    // A group or master mute can leave the instance gain nonzero.
    const float gains[] = { 0.0f, 0.0f, 1.0f, 1.0f };
    const uint32_t counts[] = { 1, 17, 65 };
    for (uint32_t channels = 1; channels <= 2; ++channels)
    {
        dmSoundCodec::Info info = {};
        info.m_Channels = channels;
        for (uint32_t d = 0; d < sizeof(deltas) / sizeof(deltas[0]); ++d)
        {
            for (uint32_t p = 0; p < sizeof(phases) / sizeof(phases[0]); ++p)
            {
                for (uint32_t n = 0; n < sizeof(counts) / sizeof(counts[0]); ++n)
                {
                    const uint32_t count = counts[n];
                    const uint32_t available = (phases[p] + count * deltas[d] + 2047) >> dmSound::RESAMPLE_FRACTION_BITS;
                    dmSound::MixContext context(0, 1, count);
                    dmSound::SoundInstance instances[2] = {};
                    dmSound::SoundGroup groups[2];
                    for (uint32_t mode = 0; mode < 2; ++mode)
                    {
                        dmSound::SoundInstance* instance = &instances[mode];
                        instance->m_FrameCount = SOUND_MAX_HISTORY + SOUND_MAX_FUTURE;
                        instance->m_FrameCapacity = 32;
                        instance->m_FrameFraction = phases[p];
                        instance->m_Gain.Reset(gains[p]);
                        instance->m_Pan.Reset(0.25f);
                        instance->m_ScaleDirty = 1;
                        instance->m_ScaleInit = (p & 1) == 0;
                        memset(decoder, 0, sizeof(decoder));
                        for (uint32_t c = 0; c < 2; ++c)
                        {
                            instance->m_ScaleL[c].Reset(1.0f);
                            instance->m_ScaleR[c].Reset(0.5f);
                            instance->m_ScaleL[c].Set(0.0f, false);
                            instance->m_ScaleR[c].Set(0.0f, false);
                            instance->m_ScaleL[c].Step();
                            instance->m_ScaleR[c].Step();
                            instance->m_Frames[c] = state[mode][c];
                            groups[mode].m_MixBuffer[c] = output[mode][c];
                            for (uint32_t i = 0; i < instance->m_FrameCount; ++i)
                            {
                                state[mode][c][i] = (float)(i + 10 * c);
                            }

                            for (uint32_t i = 0; i < count; ++i)
                            {
                                output[mode][c][i] = 37.0f;
                            }
                        }

                        // The regular mixer is the reference for cursor and history updates.
                        dmSound::Mix(&context, instance, deltas[d], available, &info, &groups[mode], mode != 0);
                    }

                    ASSERT_EQ(instances[0].m_FrameFraction, instances[1].m_FrameFraction);
                    ASSERT_EQ(instances[0].m_FrameCount, instances[1].m_FrameCount);
                    ASSERT_EQ(0U, (uint32_t)instances[1].m_ScaleDirty);
                    ASSERT_EQ(0U, (uint32_t)instances[1].m_ScaleInit);
                    for (uint32_t c = 0; c < channels; ++c)
                    {
                        ASSERT_EQ(0, memcmp(state[0][c], state[1][c], instances[0].m_FrameCount * sizeof(float)));
                        ASSERT_EQ(0, memcmp(output[0][c], output[1][c], count * sizeof(float)));
                    }

                    // Resume both paths from their saved history and compare the actual samples.
                    for (uint32_t mode = 0; mode < 2; ++mode)
                    {
                        for (uint32_t c = 0; c < 2; ++c)
                        {
                            instances[mode].m_ScaleL[c].Reset(0.25f);
                            instances[mode].m_ScaleR[c].Reset(0.5f);
                            for (uint32_t i = 0; i < 512; ++i)
                            {
                                decoder[c][i] = (float)((i * 17 + c * 11) % 97);
                            }
                        }

                        dmSound::Mix(&context, &instances[mode], deltas[d], available, &info, &groups[mode], false);
                    }

                    for (uint32_t c = 0; c < 2; ++c)
                    {
                        ASSERT_EQ(0, memcmp(output[0][c], output[1][c], count * sizeof(float)));
                    }
                }
            }
        }
    }

    dmSound::g_SoundSystem = 0;
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
