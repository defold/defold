// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeGenCommon.h"

#include <stdint.h>

namespace Luau
{
namespace CodeGen
{
namespace A64
{

enum class KindA64 : uint8_t
{
    none,
    w, // 32-bit GPR
    x, // 64-bit GPR
    s, // 32-bit SIMD&FP scalar
    d, // 64-bit SIMD&FP scalar
    q, // 128-bit SIMD&FP vector
};

struct RegisterA64
{
    KindA64 kind : 3;
    uint8_t index : 5;

    constexpr bool operator==(RegisterA64 rhs) const
    {
        return kind == rhs.kind && index == rhs.index;
    }

    constexpr bool operator!=(RegisterA64 rhs) const
    {
        return !(*this == rhs);
    }
};

constexpr RegisterA64 castReg(KindA64 kind, RegisterA64 reg)
{
    CODEGEN_ASSERT(kind != reg.kind);
    CODEGEN_ASSERT(kind != KindA64::none && reg.kind != KindA64::none);
    CODEGEN_ASSERT((kind == KindA64::w || kind == KindA64::x) == (reg.kind == KindA64::w || reg.kind == KindA64::x));

    return RegisterA64{kind, reg.index};
}

inline constexpr RegisterA64 noreg{KindA64::none, 0};

inline constexpr RegisterA64 w0{KindA64::w, 0};
inline constexpr RegisterA64 w1{KindA64::w, 1};
inline constexpr RegisterA64 w2{KindA64::w, 2};
inline constexpr RegisterA64 w3{KindA64::w, 3};
inline constexpr RegisterA64 w4{KindA64::w, 4};
inline constexpr RegisterA64 w5{KindA64::w, 5};
inline constexpr RegisterA64 w6{KindA64::w, 6};
inline constexpr RegisterA64 w7{KindA64::w, 7};
inline constexpr RegisterA64 w8{KindA64::w, 8};
inline constexpr RegisterA64 w9{KindA64::w, 9};
inline constexpr RegisterA64 w10{KindA64::w, 10};
inline constexpr RegisterA64 w11{KindA64::w, 11};
inline constexpr RegisterA64 w12{KindA64::w, 12};
inline constexpr RegisterA64 w13{KindA64::w, 13};
inline constexpr RegisterA64 w14{KindA64::w, 14};
inline constexpr RegisterA64 w15{KindA64::w, 15};
inline constexpr RegisterA64 w16{KindA64::w, 16};
inline constexpr RegisterA64 w17{KindA64::w, 17};
inline constexpr RegisterA64 w18{KindA64::w, 18};
inline constexpr RegisterA64 w19{KindA64::w, 19};
inline constexpr RegisterA64 w20{KindA64::w, 20};
inline constexpr RegisterA64 w21{KindA64::w, 21};
inline constexpr RegisterA64 w22{KindA64::w, 22};
inline constexpr RegisterA64 w23{KindA64::w, 23};
inline constexpr RegisterA64 w24{KindA64::w, 24};
inline constexpr RegisterA64 w25{KindA64::w, 25};
inline constexpr RegisterA64 w26{KindA64::w, 26};
inline constexpr RegisterA64 w27{KindA64::w, 27};
inline constexpr RegisterA64 w28{KindA64::w, 28};
inline constexpr RegisterA64 w29{KindA64::w, 29};
inline constexpr RegisterA64 w30{KindA64::w, 30};
inline constexpr RegisterA64 wzr{KindA64::w, 31};

inline constexpr RegisterA64 x0{KindA64::x, 0};
inline constexpr RegisterA64 x1{KindA64::x, 1};
inline constexpr RegisterA64 x2{KindA64::x, 2};
inline constexpr RegisterA64 x3{KindA64::x, 3};
inline constexpr RegisterA64 x4{KindA64::x, 4};
inline constexpr RegisterA64 x5{KindA64::x, 5};
inline constexpr RegisterA64 x6{KindA64::x, 6};
inline constexpr RegisterA64 x7{KindA64::x, 7};
inline constexpr RegisterA64 x8{KindA64::x, 8};
inline constexpr RegisterA64 x9{KindA64::x, 9};
inline constexpr RegisterA64 x10{KindA64::x, 10};
inline constexpr RegisterA64 x11{KindA64::x, 11};
inline constexpr RegisterA64 x12{KindA64::x, 12};
inline constexpr RegisterA64 x13{KindA64::x, 13};
inline constexpr RegisterA64 x14{KindA64::x, 14};
inline constexpr RegisterA64 x15{KindA64::x, 15};
inline constexpr RegisterA64 x16{KindA64::x, 16};
inline constexpr RegisterA64 x17{KindA64::x, 17};
inline constexpr RegisterA64 x18{KindA64::x, 18};
inline constexpr RegisterA64 x19{KindA64::x, 19};
inline constexpr RegisterA64 x20{KindA64::x, 20};
inline constexpr RegisterA64 x21{KindA64::x, 21};
inline constexpr RegisterA64 x22{KindA64::x, 22};
inline constexpr RegisterA64 x23{KindA64::x, 23};
inline constexpr RegisterA64 x24{KindA64::x, 24};
inline constexpr RegisterA64 x25{KindA64::x, 25};
inline constexpr RegisterA64 x26{KindA64::x, 26};
inline constexpr RegisterA64 x27{KindA64::x, 27};
inline constexpr RegisterA64 x28{KindA64::x, 28};
inline constexpr RegisterA64 x29{KindA64::x, 29};
inline constexpr RegisterA64 x30{KindA64::x, 30};
inline constexpr RegisterA64 xzr{KindA64::x, 31};

inline constexpr RegisterA64 sp{KindA64::none, 31};

inline constexpr RegisterA64 s0{KindA64::s, 0};
inline constexpr RegisterA64 s1{KindA64::s, 1};
inline constexpr RegisterA64 s2{KindA64::s, 2};
inline constexpr RegisterA64 s3{KindA64::s, 3};
inline constexpr RegisterA64 s4{KindA64::s, 4};
inline constexpr RegisterA64 s5{KindA64::s, 5};
inline constexpr RegisterA64 s6{KindA64::s, 6};
inline constexpr RegisterA64 s7{KindA64::s, 7};
inline constexpr RegisterA64 s8{KindA64::s, 8};
inline constexpr RegisterA64 s9{KindA64::s, 9};
inline constexpr RegisterA64 s10{KindA64::s, 10};
inline constexpr RegisterA64 s11{KindA64::s, 11};
inline constexpr RegisterA64 s12{KindA64::s, 12};
inline constexpr RegisterA64 s13{KindA64::s, 13};
inline constexpr RegisterA64 s14{KindA64::s, 14};
inline constexpr RegisterA64 s15{KindA64::s, 15};
inline constexpr RegisterA64 s16{KindA64::s, 16};
inline constexpr RegisterA64 s17{KindA64::s, 17};
inline constexpr RegisterA64 s18{KindA64::s, 18};
inline constexpr RegisterA64 s19{KindA64::s, 19};
inline constexpr RegisterA64 s20{KindA64::s, 20};
inline constexpr RegisterA64 s21{KindA64::s, 21};
inline constexpr RegisterA64 s22{KindA64::s, 22};
inline constexpr RegisterA64 s23{KindA64::s, 23};
inline constexpr RegisterA64 s24{KindA64::s, 24};
inline constexpr RegisterA64 s25{KindA64::s, 25};
inline constexpr RegisterA64 s26{KindA64::s, 26};
inline constexpr RegisterA64 s27{KindA64::s, 27};
inline constexpr RegisterA64 s28{KindA64::s, 28};
inline constexpr RegisterA64 s29{KindA64::s, 29};
inline constexpr RegisterA64 s30{KindA64::s, 30};
inline constexpr RegisterA64 s31{KindA64::s, 31};

inline constexpr RegisterA64 d0{KindA64::d, 0};
inline constexpr RegisterA64 d1{KindA64::d, 1};
inline constexpr RegisterA64 d2{KindA64::d, 2};
inline constexpr RegisterA64 d3{KindA64::d, 3};
inline constexpr RegisterA64 d4{KindA64::d, 4};
inline constexpr RegisterA64 d5{KindA64::d, 5};
inline constexpr RegisterA64 d6{KindA64::d, 6};
inline constexpr RegisterA64 d7{KindA64::d, 7};
inline constexpr RegisterA64 d8{KindA64::d, 8};
inline constexpr RegisterA64 d9{KindA64::d, 9};
inline constexpr RegisterA64 d10{KindA64::d, 10};
inline constexpr RegisterA64 d11{KindA64::d, 11};
inline constexpr RegisterA64 d12{KindA64::d, 12};
inline constexpr RegisterA64 d13{KindA64::d, 13};
inline constexpr RegisterA64 d14{KindA64::d, 14};
inline constexpr RegisterA64 d15{KindA64::d, 15};
inline constexpr RegisterA64 d16{KindA64::d, 16};
inline constexpr RegisterA64 d17{KindA64::d, 17};
inline constexpr RegisterA64 d18{KindA64::d, 18};
inline constexpr RegisterA64 d19{KindA64::d, 19};
inline constexpr RegisterA64 d20{KindA64::d, 20};
inline constexpr RegisterA64 d21{KindA64::d, 21};
inline constexpr RegisterA64 d22{KindA64::d, 22};
inline constexpr RegisterA64 d23{KindA64::d, 23};
inline constexpr RegisterA64 d24{KindA64::d, 24};
inline constexpr RegisterA64 d25{KindA64::d, 25};
inline constexpr RegisterA64 d26{KindA64::d, 26};
inline constexpr RegisterA64 d27{KindA64::d, 27};
inline constexpr RegisterA64 d28{KindA64::d, 28};
inline constexpr RegisterA64 d29{KindA64::d, 29};
inline constexpr RegisterA64 d30{KindA64::d, 30};
inline constexpr RegisterA64 d31{KindA64::d, 31};

inline constexpr RegisterA64 q0{KindA64::q, 0};
inline constexpr RegisterA64 q1{KindA64::q, 1};
inline constexpr RegisterA64 q2{KindA64::q, 2};
inline constexpr RegisterA64 q3{KindA64::q, 3};
inline constexpr RegisterA64 q4{KindA64::q, 4};
inline constexpr RegisterA64 q5{KindA64::q, 5};
inline constexpr RegisterA64 q6{KindA64::q, 6};
inline constexpr RegisterA64 q7{KindA64::q, 7};
inline constexpr RegisterA64 q8{KindA64::q, 8};
inline constexpr RegisterA64 q9{KindA64::q, 9};
inline constexpr RegisterA64 q10{KindA64::q, 10};
inline constexpr RegisterA64 q11{KindA64::q, 11};
inline constexpr RegisterA64 q12{KindA64::q, 12};
inline constexpr RegisterA64 q13{KindA64::q, 13};
inline constexpr RegisterA64 q14{KindA64::q, 14};
inline constexpr RegisterA64 q15{KindA64::q, 15};
inline constexpr RegisterA64 q16{KindA64::q, 16};
inline constexpr RegisterA64 q17{KindA64::q, 17};
inline constexpr RegisterA64 q18{KindA64::q, 18};
inline constexpr RegisterA64 q19{KindA64::q, 19};
inline constexpr RegisterA64 q20{KindA64::q, 20};
inline constexpr RegisterA64 q21{KindA64::q, 21};
inline constexpr RegisterA64 q22{KindA64::q, 22};
inline constexpr RegisterA64 q23{KindA64::q, 23};
inline constexpr RegisterA64 q24{KindA64::q, 24};
inline constexpr RegisterA64 q25{KindA64::q, 25};
inline constexpr RegisterA64 q26{KindA64::q, 26};
inline constexpr RegisterA64 q27{KindA64::q, 27};
inline constexpr RegisterA64 q28{KindA64::q, 28};
inline constexpr RegisterA64 q29{KindA64::q, 29};
inline constexpr RegisterA64 q30{KindA64::q, 30};
inline constexpr RegisterA64 q31{KindA64::q, 31};

} // namespace A64
} // namespace CodeGen
} // namespace Luau
