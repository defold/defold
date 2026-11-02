/* Generated test-only scalar subset of SIMDe.
 * Source: https://github.com/simd-everywhere/simde/tree/0ff5341927e15df0c3da8bb13bb32ec7f95aef84
 * Preprocessed with SIMDE_NO_NATIVE and SIMDE_NO_VECTOR; compiler annotations
 * and unused half/128-bit integer union fields removed.
 * Regenerate: python3 engine/dlib/src/test/simde/update_reference.py /path/to/simde
 *
 * Copyright notices from SIMDe:
 * 2015      Brandon Rowlett <browlett@nvidia.com>
 * 2015      Ken Fast <kfast@gdeb.com>
 * 2015-2017 John W. Ratcliff <jratcliffscarab@gmail.com>
 * 2016      Thomas Pornin <pornin@bolet.org>
 * 2017      Evan Nemerson <evan@nemerson.com>
 * 2017      Hasindu Gamaarachchi <hasindu@unsw.edu.au>
 * 2017-2020 Evan Nemerson <evan@nemerson.com>
 * 2018      Jeff Daily <jeff.daily@amd.com>
 * 2018-2020 Evan Nemerson <evan@nemerson.com>
 * 2019      Evan Nemerson <evan@nemerson.com>
 * 2019-2020 Michael R. Crusoe <crusoe@debian.org>
 * 2020      Ashleigh Newman-Jones <ashnewman-jones@hotmail.co.uk>
 * 2020      Christopher Moore <moore@free.fr>
 * 2020      Evan Nemerson <evan@nemerson.com>
 * 2020      Hidayat Khan <huk2209@gmail.com>
 * 2020      Himanshi Mathur <himanshi18037@iiitd.ac.in>
 * 2020      Sean Maher <seanptmaher@gmail.com>
 * 2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 * 2020      kitegi <kitegi@users.noreply.github.com>
 * 2020 Michael R. Crusoe <crusoe@debian.org>
 * 2020-2021 Christopher Moore <moore@free.fr>
 * 2020-2021 Evan Nemerson <evan@nemerson.com>
 * 2020-2022 Evan Nemerson <evan@nemerson.com>
 * 2021      Andrew Rodriguez <anrodriguez@linkedin.com>
 * 2021      Atharva Nimbalkar <atharvakn@gmail.com>
 * 2021      Christopher Moore <moore@free.fr>
 * 2021      Décio Luiz Gazzoni Filho <decio@decpp.net>
 * 2021      Evan Nemerson <evan@nemerson.com>
 * 2021      Kunwar Maheep Singh <kunwar.maheep@students.iiit.ac.in>
 * 2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 * 2022      Evan Nemerson <evan@nemerson.com>
 * 2022      Paul Saab <ps@mu.org>
 * 2023      Chi-Wei Chu <wewe5215@gapp.nthu.edu.tw> (Copyright owned by NTHU pllab)
 * 2023      Ju-Hung Li <jhlee@pllab.cs.nthu.edu.tw> (Copyright owned by NTHU pllab)
 * 2023      Michael R. Crusoe <crusoe@debian.org>
 * 2023      Yi-Yen Chung <eric681@andestech.com> (Copyright owned by Andes Technology)
 * 2023      Yung-Cheng Su <eric20607@gapp.nthu.edu.tw>
 * 2023      Yung-Cheng Su <eric20607@gapp.nthu.edu.tw> (Copyright owned by NTHU pllab)
 * 2023-2025 Michael R. Crusoe <crusoe@debian.org>
 * 2023-2026 Michael R. Crusoe <crusoe@debian.org>
 * 2024      Robin Christ <robinchrist@users.noreply.github.com>
 * 2025      Adrian Riedl <Adrian.Riedl@in.tum.de>
 * 2025      Michael R. Crusoe <crusoe@debian.org>
 * 2025      Mickey Zhu <mickey_zhu@realsil.com.cn>
 * 2026      Michael R. Crusoe <crusoe@debian.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef DM_TEST_SIMDE_REFERENCE_H
#define DM_TEST_SIMDE_REFERENCE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

typedef float  simde_float32;

typedef double simde_float64;

typedef union
{
    int8_t        i8[8];
    int16_t       i16[4];
    int32_t       i32[2];
    int64_t       i64[1];
    uint8_t       u8[8];
    uint16_t      u16[4];
    uint32_t      u32[2];
    uint64_t      u64[1];
    simde_float32 f32[2];
    int_fast32_t  i32f[8 / sizeof(int_fast32_t)];
    uint_fast32_t u32f[8 / sizeof(uint_fast32_t)];
} simde__m64_private;

typedef simde__m64_private simde__m64;

typedef union
{
    int8_t             i8[16];
    int16_t            i16[8];
    int32_t            i32[4];
    int64_t            i64[2];
    uint8_t            u8[16];
    uint16_t           u16[8];
    uint32_t           u32[4];
    uint64_t           u64[2];
    simde_float32      f32[4];
    int_fast32_t       i32f[16 / sizeof(int_fast32_t)];
    uint_fast32_t      u32f[16 / sizeof(uint_fast32_t)];
    simde__m64_private m64_private[2];
    simde__m64         m64[2];
} simde__m128_private;

typedef simde__m128_private simde__m128;

inline static simde__m128
simde__m128_from_private(simde__m128_private v)
{
    simde__m128 r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m128_private
simde__m128_to_private(simde__m128 v)
{
    simde__m128_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m128
simde_mm_set_ps(simde_float32 e3, simde_float32 e2, simde_float32 e1, simde_float32 e0)
{
    simde__m128_private r_;
    r_.f32[0] = e0;
    r_.f32[1] = e1;
    r_.f32[2] = e2;
    r_.f32[3] = e3;
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_set_ps1(simde_float32 a)
{
    return simde_mm_set_ps(a, a, a, a);
}

inline static simde__m128
simde_mm_add_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] + b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_andnot_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = ~(a_.i32[i]) & b_.i32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_xor_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = a_.u32[i] ^ b_.u32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_cmpeq_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.u32[i] = (a_.f32[i] == b_.f32[i]) ? 4294967295U : (0U);
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_cmplt_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.u32[i] = (a_.f32[i] < b_.f32[i]) ? 4294967295U : (0U);
    }
    return simde__m128_from_private(r_);
}

inline static simde_float32
simde_mm_cvtss_f32(simde__m128 a)
{
    simde__m128_private a_ = simde__m128_to_private(a);
    return a_.f32[0];
}

inline static simde__m128
simde_mm_div_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] / b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_loadu_ps(simde_float32 const mem_addr[(4)])
{
    simde__m128_private r_;
    memcpy(&r_, mem_addr, sizeof(r_));
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_max_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = (a_.f32[i] > b_.f32[i]) ? a_.f32[i] : b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_min_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = (a_.f32[i] < b_.f32[i]) ? a_.f32[i] : b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_movehl_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    r_.f32[0] = b_.f32[2];
    r_.f32[1] = b_.f32[3];
    r_.f32[2] = a_.f32[2];
    r_.f32[3] = a_.f32[3];
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_movelh_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    r_.f32[0] = a_.f32[0];
    r_.f32[1] = a_.f32[1];
    r_.f32[2] = b_.f32[0];
    r_.f32[3] = b_.f32[1];
    return simde__m128_from_private(r_);
}

inline static int
simde_mm_movemask_ps(simde__m128 a)
{
    int                 r = 0;
    simde__m128_private a_ = simde__m128_to_private(a);

    for (size_t i = 0; i < sizeof(a_.u32) / sizeof(a_.u32[0]); i++)
    {
        r |= (a_.u32[i] >> ((sizeof(a_.u32[i]) * 8) - 1)) << i;
    }
    return r;
}

inline static simde__m128
simde_mm_mul_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] * b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_setr_ps(simde_float32 e3, simde_float32 e2, simde_float32 e1, simde_float32 e0)
{
    return simde_mm_set_ps(e0, e1, e2, e3);
}

inline static simde__m128
simde_mm_shuffle_ps(simde__m128 a, simde__m128 b, const int imm8)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    r_.f32[0] = a_.f32[(imm8 >> 0) & 3];
    r_.f32[1] = a_.f32[(imm8 >> 2) & 3];
    r_.f32[2] = b_.f32[(imm8 >> 4) & 3];
    r_.f32[3] = b_.f32[(imm8 >> 6) & 3];
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_sqrt_ps(simde__m128 a)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a);

    for (size_t i = 0; i < sizeof(r_.f32) / sizeof(r_.f32[0]); i++)
    {
        r_.f32[i] = sqrtf(a_.f32[i]);
    }
    return simde__m128_from_private(r_);
}

inline static void
simde_mm_storeu_ps(simde_float32 mem_addr[4], simde__m128 a)
{
    simde__m128_private a_ = simde__m128_to_private(a);
    memcpy(mem_addr, &a_, sizeof(a_));
}

inline static simde__m128
simde_mm_sub_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] - b_.f32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_unpackhi_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    r_.f32[0] = a_.f32[2];
    r_.f32[1] = b_.f32[2];
    r_.f32[2] = a_.f32[3];
    r_.f32[3] = b_.f32[3];
    return simde__m128_from_private(r_);
}

inline static simde__m128
simde_mm_unpacklo_ps(simde__m128 a, simde__m128 b)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    r_.f32[0] = a_.f32[0];
    r_.f32[1] = b_.f32[0];
    r_.f32[2] = a_.f32[1];
    r_.f32[3] = b_.f32[1];
    return simde__m128_from_private(r_);
}

typedef union
{
    int8_t             i8[16];
    int16_t            i16[8];
    int32_t            i32[4];
    int64_t            i64[2];
    uint8_t            u8[16];
    uint16_t           u16[8];
    uint32_t           u32[4];
    uint64_t           u64[2];
    simde_float32      f32[4];
    simde_float64      f64[2];
    int_fast32_t       i32f[16 / sizeof(int_fast32_t)];
    uint_fast32_t      u32f[16 / sizeof(uint_fast32_t)];
    simde__m64_private m64_private[2];
    simde__m64         m64[2];
} simde__m128i_private;

typedef simde__m128i_private simde__m128i;

inline static simde__m128i
simde__m128i_from_private(simde__m128i_private v)
{
    simde__m128i r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m128i_private
simde__m128i_to_private(simde__m128i v)
{
    simde__m128i_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m128i
simde_mm_add_epi16(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a_.i16[i] + b_.i16[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_and_si128(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = a_.i32f[i] & b_.i32f[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_andnot_si128(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = ~(a_.i32f[i]) & b_.i32f[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_setzero_si128(void)
{
    simde__m128i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = 0;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_bsrli_si128(simde__m128i a, const int imm8)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a);
    if ((!!((imm8 & ~15))))
    {
        return simde_mm_setzero_si128();
    }

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        const int e = ((int)(i)) + imm8;
        r_.i8[i] = (e < 16) ? a_.i8[e] : 0;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_castps_si128(simde__m128 a)
{
    simde__m128i r;
    memcpy(&r, &a, sizeof(a));
    return r;
}

inline static simde__m128
simde_mm_castsi128_ps(simde__m128i a)
{
    simde__m128 r;
    memcpy(&r, &a, sizeof(a));
    return r;
}

inline static simde__m128i
simde_mm_cmpeq_epi32(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = (a_.i32[i] == b_.i32[i]) ? ~(0) : (0);
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128
simde_mm_cvtepi32_ps(simde__m128i a)
{
    simde__m128_private  r_;
    simde__m128i_private a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = (simde_float32)a_.i32[i];
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128i
simde_mm_cvtsi32_si128(int32_t a)
{
    simde__m128i_private r_;
    r_.i32[0] = a;
    r_.i32[1] = 0;
    r_.i32[2] = 0;
    r_.i32[3] = 0;
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_cvttps_epi32(simde__m128 a)
{
    simde__m128i_private r_;
    simde__m128_private  a_ = simde__m128_to_private(a);
    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        simde_float32 v = a_.f32[i];
        r_.i32[i] = ((v > ((simde_float32)((-2147483647 - 1)))) && (v < ((simde_float32)(2147483647)))) ?

        ((int32_t)((v)))

        :
        (-2147483647 - 1);
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_loadu_si128(void const* mem_addr)
{
    simde__m128i_private r_;
    memcpy(&r_, mem_addr, sizeof(r_));
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_mulhi_epu16(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = ((uint16_t)(((uint32_t)(a_.u16[i])) * ((uint32_t)(b_.u16[i])) >> 16));
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_mullo_epi16(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.u16[i] = ((uint16_t)(((uint32_t)(a_.u16[i])) * ((uint32_t)(b_.u16[i]))));
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_or_si128(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = a_.i32f[i] | b_.i32f[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_packus_epi16(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b),
    r_;

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        int16_t v = (i < (sizeof(a_.i16) / sizeof(a_.i16[0]))) ? a_.i16[i] : b_.i16[i & 7];
        r_.u8[i] = (v < 0) ? (0) : ((v > 255) ? 255 : ((uint8_t)(v)));
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_set1_epi8(int8_t a)
{
    simde__m128i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = a;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_set1_epi16(int16_t a)
{
    simde__m128i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_set1_epi32(int32_t a)
{
    simde__m128i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = a;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_sll_epi16(simde__m128i a, simde__m128i count)
{
    simde__m128i_private
    r_,
    a_,
    count_ = simde__m128i_to_private(count);
    if ((!!(count_.u64[0] > 15)))
    {
        return simde_mm_setzero_si128();
    }
    const int cnt = ((int)(count_.u64[0]));
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = ((uint16_t)((a_.u16[i] << cnt)));
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_sll_epi32(simde__m128i a, simde__m128i count)
{
    simde__m128i_private
    r_,
    a_,
    count_ = simde__m128i_to_private(count);
    if ((!!(count_.u64[0] > 31)))
    {
        return simde_mm_setzero_si128();
    }
    const int cnt = ((int)(count_.u64[0]));
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = ((uint32_t)((a_.u32[i] << cnt)));
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_srl_epi16(simde__m128i a, simde__m128i count)
{
    simde__m128i_private
    r_,
    a_,
    count_ = simde__m128i_to_private(count);
    if ((!!(count_.u64[0] > 15)))
    {
        return simde_mm_setzero_si128();
    }
    const int cnt = ((int)(count_.u64[0]));
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = a_.u16[i] >> cnt;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_srl_epi32(simde__m128i a, simde__m128i count)
{
    simde__m128i_private
    r_,
    a_,
    count_ = simde__m128i_to_private(count);
    if ((!!(count_.u64[0] > 31)))
    {
        return simde_mm_setzero_si128();
    }
    const int cnt = ((int)(count_.u64[0]));
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = a_.u32[i] >> cnt;
    }
    return simde__m128i_from_private(r_);
}

inline static void
simde_mm_storeu_si128(void* mem_addr, simde__m128i a)
{
    memcpy(mem_addr, &a, sizeof(a));
}

inline static simde__m128i
simde_mm_sub_epi16(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a_.i16[i] - b_.i16[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_unpackhi_epi8(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < ((sizeof(r_) / sizeof(r_.i8[0])) / 2); i++)
    {
        r_.i8[(i * 2)] = a_.i8[i + ((sizeof(r_) / sizeof(r_.i8[0])) / 2)];
        r_.i8[(i * 2) + 1] = b_.i8[i + ((sizeof(r_) / sizeof(r_.i8[0])) / 2)];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_unpacklo_epi8(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < ((sizeof(r_) / sizeof(r_.i8[0])) / 2); i++)
    {
        r_.i8[(i * 2)] = a_.i8[i];
        r_.i8[(i * 2) + 1] = b_.i8[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_xor_si128(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = a_.i32f[i] ^ b_.i32f[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_shuffle_epi8(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);
    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = a_.i8[b_.i8[i] & 15] & (~(b_.i8[i]) >> 7);
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_cvtepu8_epi16(simde__m128i a)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a_.u8[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_cvtepu16_epi32(simde__m128i a)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = a_.u16[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128
simde_mm_dp_ps(simde__m128 a, simde__m128 b, const int imm8)
{
    simde__m128_private
    r_,
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b);
    simde_float32 sum = 0.0f;

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        sum += ((imm8 >> (i + 4)) & 1) ? (a_.f32[i] * b_.f32[i]) : 0.0f;
    }

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = ((imm8 >> i) & 1) ? sum : 0.0f;
    }
    return simde__m128_from_private(r_);
}

inline static simde__m128i
simde_mm_max_epu32(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = a_.u32[i] > b_.u32[i] ? a_.u32[i] : b_.u32[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_min_epu32(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = a_.u32[i] < b_.u32[i] ? a_.u32[i] : b_.u32[i];
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m128i
simde_mm_mullo_epi32(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.u32[i] = ((uint32_t)((((uint64_t)((((int64_t)(a_.i32[i])) * ((int64_t)(b_.i32[i]))))) & 0xffffffff)));
    }
    return simde__m128i_from_private(r_);
}

typedef union
{
    int8_t              i8[32];
    int16_t             i16[16];
    int32_t             i32[8];
    int64_t             i64[4];
    uint8_t             u8[32];
    uint16_t            u16[16];
    uint32_t            u32[8];
    uint64_t            u64[4];
    int_fast32_t        i32f[32 / sizeof(int_fast32_t)];
    uint_fast32_t       u32f[32 / sizeof(uint_fast32_t)];
    simde_float32       f32[8];
    simde_float64       f64[4];
    simde__m128_private m128_private[2];
    simde__m128         m128[2];
} simde__m256_private;

typedef union
{
    int8_t               i8[32];
    int16_t              i16[16];
    int32_t              i32[8];
    int64_t              i64[4];
    uint8_t              u8[32];
    uint16_t             u16[16];
    uint32_t             u32[8];
    uint64_t             u64[4];
    int_fast32_t         i32f[32 / sizeof(int_fast32_t)];
    uint_fast32_t        u32f[32 / sizeof(uint_fast32_t)];
    simde_float32        f32[8];
    simde_float64        f64[4];
    simde__m128i_private m128i_private[2];
    simde__m128i         m128i[2];
} simde__m256i_private;

typedef simde__m256_private  simde__m256;

typedef simde__m256i_private simde__m256i;

inline static simde__m256
simde__m256_from_private(simde__m256_private v)
{
    simde__m256 r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m256_private
simde__m256_to_private(simde__m256 v)
{
    simde__m256_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m256i
simde__m256i_from_private(simde__m256i_private v)
{
    simde__m256i r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m256i_private
simde__m256i_to_private(simde__m256i v)
{
    simde__m256i_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m256i
simde_mm256_setzero_si256(void)
{
    simde__m256i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = 0;
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_set1_epi8(int8_t a)
{
    simde__m256i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = a;
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_set1_epi16(int16_t a)
{
    simde__m256i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a;
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_set1_epi32(int32_t a)
{
    simde__m256i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = a;
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256
simde_mm256_set1_ps(simde_float32 a)
{
    simde__m256_private r_;

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a;
    }
    return simde__m256_from_private(r_);
}

inline static simde__m256
simde_mm256_add_ps(simde__m256 a, simde__m256 b)
{
    simde__m256_private
    r_,
    a_ = simde__m256_to_private(a),
    b_ = simde__m256_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] + b_.f32[i];
    }
    return simde__m256_from_private(r_);
}

inline static simde__m256
simde_mm256_loadu_ps(const float a[(8)])
{
    simde__m256 r;
    memcpy(&r, a, sizeof(r));
    return r;
}

inline static simde__m256i
simde_mm256_loadu_si256(void const* mem_addr)
{
    simde__m256i r;
    memcpy(&r, mem_addr, sizeof(r));
    return r;
}

inline static simde__m256
simde_mm256_mul_ps(simde__m256 a, simde__m256 b)
{
    simde__m256_private
    r_,
    a_ = simde__m256_to_private(a),
    b_ = simde__m256_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a_.f32[i] * b_.f32[i];
    }
    return simde__m256_from_private(r_);
}

inline static void
simde_mm256_storeu_ps(simde_float32 mem_addr[8], simde__m256 a)
{
    memcpy(mem_addr, &a, sizeof(a));
}

inline static void
simde_mm256_storeu_si256(void* mem_addr, simde__m256i a)
{
    memcpy(mem_addr, &a, sizeof(a));
}

typedef union
{
    int8_t              i8[64];
    int16_t             i16[32];
    int32_t             i32[16];
    int64_t             i64[8];
    uint8_t             u8[64];
    uint16_t            u16[32];
    uint32_t            u32[16];
    uint64_t            u64[8];
    int_fast32_t        i32f[64 / sizeof(int_fast32_t)];
    uint_fast32_t       u32f[64 / sizeof(uint_fast32_t)];
    simde_float32       f32[16];
    simde_float64       f64[8];
    simde__m128_private m128_private[4];
    simde__m128         m128[4];
    simde__m256_private m256_private[2];
    simde__m256         m256[2];
} simde__m512_private;

typedef union
{
    int8_t               i8[64];
    int16_t              i16[32];
    int32_t              i32[16];
    int64_t              i64[8];
    uint8_t              u8[64];
    uint16_t             u16[32];
    uint32_t             u32[16];
    uint64_t             u64[8];
    int_fast32_t         i32f[64 / sizeof(int_fast32_t)];
    uint_fast32_t        u32f[64 / sizeof(uint_fast32_t)];
    simde_float32        f32[16];
    simde_float64        f64[8];
    simde__m128i_private m128i_private[4];
    simde__m128i         m128i[4];
    simde__m256i_private m256i_private[2];
    simde__m256i         m256i[2];
} simde__m512i_private;

typedef simde__m512_private  simde__m512;

typedef simde__m512i_private simde__m512i;

typedef uint64_t             simde__mmask64;

inline static simde__m512
simde__m512_from_private(simde__m512_private v)
{
    simde__m512 r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m512_private
simde__m512_to_private(simde__m512 v)
{
    simde__m512_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m512i
simde__m512i_from_private(simde__m512i_private v)
{
    simde__m512i r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m512i_private
simde__m512i_to_private(simde__m512i v)
{
    simde__m512i_private r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline static simde__m512i
simde_mm512_maskz_mov_epi8(simde__mmask64 k, simde__m512i a)
{
    simde__m512i_private
    a_ = simde__m512i_to_private(a),
    r_;
    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : (0);
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_set1_epi8(int8_t a)
{
    simde__m512i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = a;
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_set1_epi16(int16_t a)
{
    simde__m512i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a;
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_set1_epi32(int32_t a)
{
    simde__m512i_private r_;

    for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++)
    {
        r_.i32[i] = a;
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512
simde_mm512_set1_ps(simde_float32 a)
{
    simde__m512_private r_;

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = a;
    }
    return simde__m512_from_private(r_);
}

inline static simde__m256i
simde_mm256_add_epi16(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = a_.i16[i] + b_.i16[i];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_cmpgt_epi8(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = (a_.i8[i] > b_.i8[i]) ? ~(0) : (0);
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_min_epu16(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = (a_.u16[i] < b_.u16[i]) ? a_.u16[i] : b_.u16[i];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_mullo_epi16(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b),
    r_;

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = ((int16_t)(a_.i16[i] * b_.i16[i]));
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_or_si256(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = a_.i32f[i] | b_.i32f[i];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_packus_epi16(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);
    const size_t halfway_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 2;
    const size_t quarter_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 4;

    for (size_t i = 0; i < quarter_point; i++)
    {
        r_.u8[i] = (a_.i16[i] > 255) ? 255 : ((a_.i16[i] < 0) ? (0) : ((uint8_t)(a_.i16[i])));
        r_.u8[i + quarter_point] = (b_.i16[i] > 255) ? 255 : ((b_.i16[i] < 0) ? (0) : ((uint8_t)(b_.i16[i])));
        r_.u8[halfway_point + i] = (a_.i16[quarter_point + i] > 255) ? 255 : ((a_.i16[quarter_point + i] < 0) ? (0) : ((uint8_t)(a_.i16[quarter_point + i])));
        r_.u8[halfway_point + i + quarter_point] = (b_.i16[quarter_point + i] > 255) ? 255 : ((b_.i16[quarter_point + i] < 0) ? (0) : ((uint8_t)(b_.i16[quarter_point + i])));
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_shuffle_epi8(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < ((sizeof(r_.u8) / sizeof(r_.u8[0])) / 2); i++)
    {
        r_.u8[i] = (b_.u8[i] & 0x80) ? 0 : a_.u8[(b_.u8[i] & 0x0f)];
        r_.u8[i + 16] = (b_.u8[i + 16] & 0x80) ? 0 : a_.u8[(b_.u8[i + 16] & 0x0f) + 16];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_srl_epi16(simde__m256i a, simde__m128i count)
{
    simde__m256i_private r_, a_;
    simde__m128i_private count_ = simde__m128i_to_private(count);
    if ((!!(count_.u64[0] > 15)))
    {
        return simde_mm256_setzero_si256();
    }
    const int cnt = ((int)(count_.u64[0]));
    a_ = simde__m256i_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = a_.u16[i] >> cnt;
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m128i
simde_mm_srlv_epi32(simde__m128i a, simde__m128i b)
{
    simde__m128i_private
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b),
    r_;

    for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++)
    {
        r_.u32[i] = (b_.u32[i] < 32) ? (a_.u32[i] >> b_.u32[i]) : 0;
    }
    return simde__m128i_from_private(r_);
}

inline static simde__m256i
simde_mm256_unpacklo_epi8(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0]) / 2); i++)
    {
        r_.i8[2 * i] = a_.i8[i + ~(~i | 7)];
        r_.i8[2 * i + 1] = b_.i8[i + ~(~i | 7)];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m256i
simde_mm256_unpackhi_epi8(simde__m256i a, simde__m256i b)
{
    simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0]) / 2); i++)
    {
        r_.i8[2 * i] = a_.i8[i + 8 + ~(~i | 7)];
        r_.i8[2 * i + 1] = b_.i8[i + 8 + ~(~i | 7)];
    }
    return simde__m256i_from_private(r_);
}

inline static simde__m512i
simde_mm512_add_epi16(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])); i++)
    {
        r_.m256i[i] = simde_mm256_add_epi16(a_.m256i[i], b_.m256i[i]);
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512
simde_mm512_add_ps(simde__m512 a, simde__m512 b)
{
    simde__m512_private
    r_,
    a_ = simde__m512_to_private(a),
    b_ = simde__m512_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.m256) / sizeof(r_.m256[0])); i++)
    {
        r_.m256[i] = simde_mm256_add_ps(a_.m256[i], b_.m256[i]);
    }
    return simde__m512_from_private(r_);
}

inline static simde__m512i
simde_mm512_setzero_si512(void)
{
    simde__m512i r;
    memset(&r, 0, sizeof(r));
    return r;
}

inline static simde__mmask64
simde_mm512_cmplt_epu8_mask(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);
    simde__mmask64 r = 0;

    for (size_t i = 0; i < (sizeof(a_.u8) / sizeof(a_.u8[0])); i++)
    {
        r |= (a_.u8[i] < b_.u8[i]) ? ((1ULL) << i) : 0;
    }
    return r;
}

inline static simde__m512
simde_mm512_loadu_ps(void const* mem_addr)
{
    simde__m512 r;
    memcpy(&r, mem_addr, sizeof(r));
    return r;
}

inline static simde__m512i
simde_mm512_loadu_si512(void const* mem_addr)
{
    simde__m512i r;
    memcpy(&r, mem_addr, sizeof(r));
    return r;
}

inline static void
simde_mm512_storeu_ps(void* mem_addr, simde__m512 a)
{
    memcpy(mem_addr, &a, sizeof(a));
}

inline static void
simde_mm512_storeu_si512(void* mem_addr, simde__m512i a)
{
    memcpy(mem_addr, &a, sizeof(a));
}

inline static simde__m512i
simde_mm512_shuffle_epi8(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++)
    {
        r_.i8[i] = (b_.i8[i] & 0x80) ? 0 : a_.i8[(b_.i8[i] & 0x0f) + (i & 0x30)];
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_maskz_shuffle_epi8(simde__mmask64 k, simde__m512i a, simde__m512i b)
{
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_shuffle_epi8(a, b));
}

inline static simde__m128
simde_mm_fmadd_ps(simde__m128 a, simde__m128 b, simde__m128 c)
{
    simde__m128_private
    a_ = simde__m128_to_private(a),
    b_ = simde__m128_to_private(b),
    c_ = simde__m128_to_private(c),
    r_;

    for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++)
    {
        r_.f32[i] = fmaf(a_.f32[i], b_.f32[i], c_.f32[i]);
    }
    return simde__m128_from_private(r_);
}

inline static simde__m512
simde_mm512_mul_ps(simde__m512 a, simde__m512 b)
{
    simde__m512_private
    r_,
    a_ = simde__m512_to_private(a),
    b_ = simde__m512_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.m256) / sizeof(r_.m256[0])); i++)
    {
        r_.m256[i] = simde_mm256_mul_ps(a_.m256[i], b_.m256[i]);
    }
    return simde__m512_from_private(r_);
}

inline static simde__m512i
simde_mm512_min_epu16(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++)
    {
        r_.u16[i] = (a_.u16[i] < b_.u16[i]) ? a_.u16[i] : b_.u16[i];
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_mullo_epi16(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b),
    r_;

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.i16[i] = ((int16_t)(a_.i16[i] * b_.i16[i]));
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_or_si512(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])); i++)
    {
        r_.i32f[i] = a_.i32f[i] | b_.i32f[i];
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_packus_epi16(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);
    const size_t halfway_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 2;
    const size_t quarter_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 4;
    const size_t octet_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 8;

    for (size_t i = 0; i < octet_point; i++)
    {
        r_.u8[i] = (a_.i16[i] > 255) ? 255 : ((a_.i16[i] < 0) ? (0) : ((uint8_t)(a_.i16[i])));
        r_.u8[i + octet_point] = (b_.i16[i] > 255) ? 255 : ((b_.i16[i] < 0) ? (0) : ((uint8_t)(b_.i16[i])));
        r_.u8[quarter_point + i] = (a_.i16[octet_point + i] > 255) ? 255 : ((a_.i16[octet_point + i] < 0) ? (0) : ((uint8_t)(a_.i16[octet_point + i])));
        r_.u8[quarter_point + i + octet_point] = (b_.i16[octet_point + i] > 255) ? 255 : ((b_.i16[octet_point + i] < 0) ? (0) : ((uint8_t)(b_.i16[octet_point + i])));
        r_.u8[halfway_point + i] = (a_.i16[quarter_point + i] > 255) ? 255 : ((a_.i16[quarter_point + i] < 0) ? (0) : ((uint8_t)(a_.i16[quarter_point + i])));
        r_.u8[halfway_point + i + octet_point] = (b_.i16[quarter_point + i] > 255) ? 255 : ((b_.i16[quarter_point + i] < 0) ? (0) : ((uint8_t)(b_.i16[quarter_point + i])));
        r_.u8[halfway_point + quarter_point + i] = (a_.i16[quarter_point + octet_point + i] > 255) ? 255 : ((a_.i16[quarter_point + octet_point + i] < 0) ? (0) : ((uint8_t)(a_.i16[quarter_point + octet_point + i])));
        r_.u8[halfway_point + quarter_point + i + octet_point] = (b_.i16[quarter_point + octet_point + i] > 255) ? 255 : ((b_.i16[quarter_point + octet_point + i] < 0) ? (0) : ((uint8_t)(b_.i16[quarter_point + octet_point + i])));
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_srl_epi16(simde__m512i a, simde__m128i count)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a);
    simde__m128i_private
    count_ = simde__m128i_to_private(count);
    if (((uint64_t)(count_.i64[0])) > 15)
        return simde_mm512_setzero_si512();

    for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++)
    {
        r_.u16[i] = a_.u16[i] >> count_.i64[0];
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_unpacklo_epi8(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0]) / 2); i++)
    {
        r_.i8[2 * i] = a_.i8[i + ~(~i | 7)];
        r_.i8[2 * i + 1] = b_.i8[i + ~(~i | 7)];
    }
    return simde__m512i_from_private(r_);
}

inline static simde__m512i
simde_mm512_unpackhi_epi8(simde__m512i a, simde__m512i b)
{
    simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0]) / 2); i++)
    {
        r_.i8[2 * i] = a_.i8[i + 8 + ~(~i | 7)];
        r_.i8[2 * i + 1] = b_.i8[i + 8 + ~(~i | 7)];
    }
    return simde__m512i_from_private(r_);
}

typedef union
{
    uint8_t values[(8) / sizeof(uint8_t)];
} simde_uint8x8_private;

typedef union
{
    uint16_t values[(8) / sizeof(uint16_t)];
} simde_uint16x4_private;

typedef union
{
    uint8_t values[(16) / sizeof(uint8_t)];
} simde_uint8x16_private;

typedef union
{
    uint16_t values[(16) / sizeof(uint16_t)];
} simde_uint16x8_private;

typedef union
{
    uint32_t values[(16) / sizeof(uint32_t)];
} simde_uint32x4_private;

typedef union
{
    simde_float32 values[(16) / sizeof(simde_float32)];
} simde_float32x4_private;

typedef simde_uint8x8_private   simde_uint8x8_t;

typedef simde_uint16x4_private  simde_uint16x4_t;

typedef simde_uint8x16_private  simde_uint8x16_t;

typedef simde_uint16x8_private  simde_uint16x8_t;

typedef simde_uint32x4_private  simde_uint32x4_t;

typedef simde_float32x4_private simde_float32x4_t;

typedef simde_float32           simde_float32_t;

typedef struct simde_uint16x8x2_t
{
    simde_uint16x8_t val[2];
} simde_uint16x8x2_t;

typedef struct simde_uint8x16x4_t
{
    simde_uint8x16_t val[4];
} simde_uint8x16x4_t;

static inline simde_uint8x8_private simde_uint8x8_to_private(simde_uint8x8_t value)
{
    simde_uint8x8_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint8x8_t simde_uint8x8_from_private(simde_uint8x8_private value)
{
    simde_uint8x8_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint16x4_private simde_uint16x4_to_private(simde_uint16x4_t value)
{
    simde_uint16x4_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint16x4_t simde_uint16x4_from_private(simde_uint16x4_private value)
{
    simde_uint16x4_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint8x16_private simde_uint8x16_to_private(simde_uint8x16_t value)
{
    simde_uint8x16_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint8x16_t simde_uint8x16_from_private(simde_uint8x16_private value)
{
    simde_uint8x16_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint16x8_private simde_uint16x8_to_private(simde_uint16x8_t value)
{
    simde_uint16x8_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint16x8_t simde_uint16x8_from_private(simde_uint16x8_private value)
{
    simde_uint16x8_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint32x4_private simde_uint32x4_to_private(simde_uint32x4_t value)
{
    simde_uint32x4_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_uint32x4_t simde_uint32x4_from_private(simde_uint32x4_private value)
{
    simde_uint32x4_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_float32x4_private simde_float32x4_to_private(simde_float32x4_t value)
{
    simde_float32x4_private r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

static inline simde_float32x4_t simde_float32x4_from_private(simde_float32x4_private value)
{
    simde_float32x4_t r;
    memcpy(&r, &value, sizeof(r));
    return r;
}

inline static simde_uint32x4_t
simde_vsubq_u32(simde_uint32x4_t a, simde_uint32x4_t b)
{
    simde_uint32x4_private
    r_,
    a_ = simde_uint32x4_to_private(a),
    b_ = simde_uint32x4_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = a_.values[i] - b_.values[i];
    }
    return simde_uint32x4_from_private(r_);
}

inline static simde_uint8x16_t
simde_vcombine_u8(simde_uint8x8_t low, simde_uint8x8_t high)
{
    simde_uint8x16_private r_;
    simde_uint8x8_private
    low_ = simde_uint8x8_to_private(low),
    high_ = simde_uint8x8_to_private(high);
    size_t halfway = (sizeof(r_.values) / sizeof(r_.values[0])) / 2;

    for (size_t i = 0; i < halfway; i++)
    {
        r_.values[i] = low_.values[i];
        r_.values[i + halfway] = high_.values[i];
    }
    return simde_uint8x16_from_private(r_);
}

inline static simde_uint16x8_t
simde_vcombine_u16(simde_uint16x4_t low, simde_uint16x4_t high)
{
    simde_uint16x8_private r_;
    simde_uint16x4_private
    low_ = simde_uint16x4_to_private(low),
    high_ = simde_uint16x4_to_private(high);
    size_t halfway = (sizeof(r_.values) / sizeof(r_.values[0])) / 2;

    for (size_t i = 0; i < halfway; i++)
    {
        r_.values[i] = low_.values[i];
        r_.values[i + halfway] = high_.values[i];
    }
    return simde_uint16x8_from_private(r_);
}

inline static simde_uint32x4_t
simde_vaddq_u32(simde_uint32x4_t a, simde_uint32x4_t b)
{
    simde_uint32x4_private
    r_,
    a_ = simde_uint32x4_to_private(a),
    b_ = simde_uint32x4_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = a_.values[i] + b_.values[i];
    }
    return simde_uint32x4_from_private(r_);
}

inline static simde_uint32x4_t
simde_vdupq_n_u32(uint32_t value)
{
    simde_uint32x4_private r_;

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = value;
    }
    return simde_uint32x4_from_private(r_);
}

inline static simde_uint32x4_t
simde_vcltq_u32(simde_uint32x4_t a, simde_uint32x4_t b)
{
    simde_uint32x4_private
    r_,
    a_ = simde_uint32x4_to_private(a),
    b_ = simde_uint32x4_to_private(b);

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? 4294967295U : 0;
    }
    return simde_uint32x4_from_private(r_);
}

inline static simde_float32
simde_vcvts_f32_u32(uint32_t a)
{
    return ((simde_float32)(a));
}

inline static simde_float32x4_t
simde_vcvtq_f32_u32(simde_uint32x4_t a)
{
    simde_uint32x4_private  a_ = simde_uint32x4_to_private(a);
    simde_float32x4_private r_;

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = simde_vcvts_f32_u32(a_.values[i]);
    }
    return simde_float32x4_from_private(r_);
}

inline static simde_uint16x8_t
simde_vld1q_u16(uint16_t const ptr[(8)])
{
    simde_uint16x8_private r_;
    memcpy(&r_, ptr, sizeof(r_));
    return simde_uint16x8_from_private(r_);
}

inline static simde_uint32x4_t
simde_vld1q_u32(uint32_t const ptr[(4)])
{
    simde_uint32x4_private r_;
    memcpy(&r_, ptr, sizeof(r_));
    return simde_uint32x4_from_private(r_);
}

inline static simde_uint16x8x2_t
simde_vld2q_u16(uint16_t const ptr[(16)])
{
    simde_uint16x8_private r_[2];
    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++)
    {
        for (size_t j = 0; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])); j++)
        {
            r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
        }
    }
    simde_uint16x8x2_t r = { {
    simde_uint16x8_from_private(r_[0]),
    simde_uint16x8_from_private(r_[1]),
    } };
    return r;
}

inline static simde_uint8x16x4_t
simde_vld4q_u8(uint8_t const ptr[(64)])
{
    simde_uint8x16_private a_[4];
    for (size_t i = 0; i < (sizeof(simde_uint8x16_t) / sizeof(*ptr)) * 4; i++)
    {
        a_[i % 4].values[i / 4] = ptr[i];
    }
    simde_uint8x16x4_t s_ = { { simde_uint8x16_from_private(a_[0]), simde_uint8x16_from_private(a_[1]), simde_uint8x16_from_private(a_[2]), simde_uint8x16_from_private(a_[3]) } };
    return s_;
}

inline static uint8_t
simde_vqmovnh_u16(uint16_t a)
{
    return (a > 255) ? 255 : ((uint8_t)(a));
}

inline static uint16_t
simde_vqmovns_u32(uint32_t a)
{
    return (a > 65535) ? 65535 : ((uint16_t)(a));
}

inline static simde_uint8x8_t
simde_vqmovn_u16(simde_uint16x8_t a)
{
    simde_uint8x8_private  r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = simde_vqmovnh_u16(a_.values[i]);
    }
    return simde_uint8x8_from_private(r_);
}

inline static simde_uint16x4_t
simde_vqmovn_u32(simde_uint32x4_t a)
{
    simde_uint16x4_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);

    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++)
    {
        r_.values[i] = simde_vqmovns_u32(a_.values[i]);
    }
    return simde_uint16x4_from_private(r_);
}

inline static void
simde_vst1q_f32(simde_float32_t ptr[(4)], simde_float32x4_t val)
{
    simde_float32x4_private val_ = simde_float32x4_to_private(val);
    memcpy(ptr, &val_, sizeof(val_));
}

inline static void
simde_vst1q_u8(uint8_t ptr[(16)], simde_uint8x16_t val)
{
    simde_uint8x16_private val_ = simde_uint8x16_to_private(val);
    memcpy(ptr, &val_, sizeof(val_));
}

inline static void
simde_vst1q_u16(uint16_t ptr[(8)], simde_uint16x8_t val)
{
    simde_uint16x8_private val_ = simde_uint16x8_to_private(val);
    memcpy(ptr, &val_, sizeof(val_));
}

inline static void
simde_vst1q_u32(uint32_t ptr[(4)], simde_uint32x4_t val)
{
    simde_uint32x4_private val_ = simde_uint32x4_to_private(val);
    memcpy(ptr, &val_, sizeof(val_));
}

inline static void
simde_vst4q_u8(uint8_t* ptr, simde_uint8x16x4_t val)
{
    simde_uint8x16_private a_[4] = { simde_uint8x16_to_private(val.val[0]), simde_uint8x16_to_private(val.val[1]), simde_uint8x16_to_private(val.val[2]), simde_uint8x16_to_private(val.val[3]) };
    uint8_t                buf[64];
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 4; i++)
    {
        buf[i] = a_[i % 4].values[i / 4];
    }
    memcpy(ptr, buf, sizeof(buf));
}

#define simde_mm_set1_ps simde_mm_set_ps1
#define simde_mm_srli_si128 simde_mm_bsrli_si128
#define SIMDE_MM_TRANSPOSE4_PS(row0, row1, row2, row3) \
    do \
    { \
        simde__m128 tmp0 = simde_mm_unpacklo_ps(row0, row1); \
        simde__m128 tmp1 = simde_mm_unpackhi_ps(row0, row1); \
        simde__m128 tmp2 = simde_mm_unpacklo_ps(row2, row3); \
        simde__m128 tmp3 = simde_mm_unpackhi_ps(row2, row3); \
        row0 = simde_mm_movelh_ps(tmp0, tmp2); \
        row1 = simde_mm_movehl_ps(tmp2, tmp0); \
        row2 = simde_mm_movelh_ps(tmp1, tmp3); \
        row3 = simde_mm_movehl_ps(tmp3, tmp1); \
    } while (0)

#endif
