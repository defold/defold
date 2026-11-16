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

#include <simd/simd_fixed_float.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static unsigned g_Checks;
static unsigned g_Failures;

static inline void Check(int condition) __arm_streaming_compatible
{
    ++g_Checks;
    if (!condition)
    {
        ++g_Failures;
    }
}

// Verifies native sixteen-lane arithmetic, exact transfers and PCM boundary semantics.
__arm_locally_streaming static void TestSMEPrimitives(void)
{
    float values[32], output[32];
    for (unsigned i = 0; i < 32; ++i)
    {
        values[i] = (float)i - 16.0f;
    }

    dsimd_float16_t a = Simd16Load(values);
    dsimd_float16_t b = Simd16Load(values + 16);
    Simd16Store(output, Simd16Add(a, b));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == values[i] + values[i + 16]);
        Check(Simd16Lane(a, i) == values[i]);
    }

    Simd16Store(output, Simd16Mul(a, Simd16Splat(0.5f)));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == values[i] * 0.5f);
    }

    Simd16Store(output, Simd16Min(a, b));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == values[i]);
    }

    Simd16Store(output, Simd16Max(a, b));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == values[i + 16]);
    }

    Simd16Store(output, Simd16Ramp(0.731f, -0.001f));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == 0.731f + (float)i * -0.001f);
    }

    Check(Simd16Sum(a) == -136.0f);
    Check(Simd16ReduceMax(a) == -1.0f);

    dsimd_float4_t blocks[4];
    memcpy(blocks, values, sizeof(blocks));
    Simd16Store(output, Simd16Combine(blocks[0], blocks[1], blocks[2], blocks[3]));
    Check(memcmp(output, values, 64) == 0);
    dsimd_float16_t left, right;
    Simd16Unzip(a, b, &left, &right);
    Simd16Store(output, left);
    Simd16Store(output + 16, right);
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == values[i * 2]);
        Check(output[i + 16] == values[i * 2 + 1]);
    }

    for (unsigned base = 0; base < 256; base += 16)
    {
        uint8_t bytes[16];
        for (unsigned i = 0; i < 16; ++i)
        {
            bytes[i] = (uint8_t)(base + i);
        }

        Simd16Store(output, Simd16LoadS8ToS16Float(bytes));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(output[i] == (float)(int16_t)((uint16_t)bytes[i] * 257u));
        }
    }

    for (unsigned base = 0; base < 65536; base += 16)
    {
        int16_t words[16];
        for (unsigned i = 0; i < 16; ++i)
        {
            words[i] = (int16_t)(base + i);
        }

        Simd16Store(output, Simd16LoadS16ToFloat(words));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(output[i] == (float)words[i]);
        }
    }

    const float extremes[] = {
        -1.0e30f, -32768.75f, -1.75f, -0.0f, 0.75f, 1.75f, 32767.75f, 1.0e30f
    };
    const int16_t expected[] = {
        -32768, -32768, -1, 0, 0, 1, 32767, 32767
    };
    for (unsigned i = 0; i < 32; ++i)
    {
        values[i] = extremes[i % 8];
    }

    uint8_t packed[66];
    memset(packed, 123, sizeof(packed));
    Simd16StoreInterleavedS16(packed + 1, Simd16Load(values), Simd16Load(values + 16));
    Check(packed[0] == 123 && packed[65] == 123);
    int16_t samples[32];
    memcpy(samples, packed + 1, 64);
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(samples[i * 2] == expected[i % 8]);
        Check(samples[i * 2 + 1] == expected[i % 8]);
    }
}

// Verifies partial loads stop at a protected page, including zero and every tail length.
__arm_locally_streaming static void TestSMEPartialLoads(float* end)
{
    float output[16];
    for (unsigned count = 0; count <= 16; ++count)
    {
        float* source = end - count;
        for (unsigned i = 0; i < count; ++i)
        {
            source[i] = (float)i - 8.25f;
        }

        Simd16Store(output, Simd16LoadPartial(source, count));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(output[i] == (i < count ? source[i] : 0.0f));
        }
    }
}

// Runs only after baseline code verifies SME support and a 512-bit streaming length.
int SimdTestSME(void)
{
    TestSMEPrimitives();
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    void* pages = mmap(0, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (pages == MAP_FAILED)
    {
        return 1;
    }

    if (mprotect((uint8_t*)pages + page_size, page_size, PROT_NONE) != 0)
    {
        munmap(pages, page_size * 2);
        return 1;
    }

    TestSMEPartialLoads((float*)((uint8_t*)pages + page_size));
    munmap(pages, page_size * 2);
    printf("SME primitives: %u checks, %u failures\n", g_Checks, g_Failures);
    return g_Failures ? 1 : 0;
}
