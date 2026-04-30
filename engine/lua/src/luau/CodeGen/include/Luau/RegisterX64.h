// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeGenCommon.h"

#include <stdint.h>

namespace Luau
{
namespace CodeGen
{
namespace X64
{

enum class SizeX64 : uint8_t
{
    none,
    byte,
    word,
    dword,
    qword,
    xmmword,
    ymmword,
};

struct RegisterX64
{
    SizeX64 size : 3;
    uint8_t index : 5;

    constexpr bool operator==(RegisterX64 rhs) const
    {
        return size == rhs.size && index == rhs.index;
    }

    constexpr bool operator!=(RegisterX64 rhs) const
    {
        return !(*this == rhs);
    }
};

inline constexpr RegisterX64 noreg{SizeX64::none, 16};
inline constexpr RegisterX64 rip{SizeX64::none, 0};

inline constexpr RegisterX64 al{SizeX64::byte, 0};
inline constexpr RegisterX64 cl{SizeX64::byte, 1};
inline constexpr RegisterX64 dl{SizeX64::byte, 2};
inline constexpr RegisterX64 bl{SizeX64::byte, 3};
inline constexpr RegisterX64 spl{SizeX64::byte, 4};
inline constexpr RegisterX64 bpl{SizeX64::byte, 5};
inline constexpr RegisterX64 sil{SizeX64::byte, 6};
inline constexpr RegisterX64 dil{SizeX64::byte, 7};
inline constexpr RegisterX64 r8b{SizeX64::byte, 8};
inline constexpr RegisterX64 r9b{SizeX64::byte, 9};
inline constexpr RegisterX64 r10b{SizeX64::byte, 10};
inline constexpr RegisterX64 r11b{SizeX64::byte, 11};
inline constexpr RegisterX64 r12b{SizeX64::byte, 12};
inline constexpr RegisterX64 r13b{SizeX64::byte, 13};
inline constexpr RegisterX64 r14b{SizeX64::byte, 14};
inline constexpr RegisterX64 r15b{SizeX64::byte, 15};

inline constexpr RegisterX64 eax{SizeX64::dword, 0};
inline constexpr RegisterX64 ecx{SizeX64::dword, 1};
inline constexpr RegisterX64 edx{SizeX64::dword, 2};
inline constexpr RegisterX64 ebx{SizeX64::dword, 3};
inline constexpr RegisterX64 esp{SizeX64::dword, 4};
inline constexpr RegisterX64 ebp{SizeX64::dword, 5};
inline constexpr RegisterX64 esi{SizeX64::dword, 6};
inline constexpr RegisterX64 edi{SizeX64::dword, 7};
inline constexpr RegisterX64 r8d{SizeX64::dword, 8};
inline constexpr RegisterX64 r9d{SizeX64::dword, 9};
inline constexpr RegisterX64 r10d{SizeX64::dword, 10};
inline constexpr RegisterX64 r11d{SizeX64::dword, 11};
inline constexpr RegisterX64 r12d{SizeX64::dword, 12};
inline constexpr RegisterX64 r13d{SizeX64::dword, 13};
inline constexpr RegisterX64 r14d{SizeX64::dword, 14};
inline constexpr RegisterX64 r15d{SizeX64::dword, 15};

inline constexpr RegisterX64 rax{SizeX64::qword, 0};
inline constexpr RegisterX64 rcx{SizeX64::qword, 1};
inline constexpr RegisterX64 rdx{SizeX64::qword, 2};
inline constexpr RegisterX64 rbx{SizeX64::qword, 3};
inline constexpr RegisterX64 rsp{SizeX64::qword, 4};
inline constexpr RegisterX64 rbp{SizeX64::qword, 5};
inline constexpr RegisterX64 rsi{SizeX64::qword, 6};
inline constexpr RegisterX64 rdi{SizeX64::qword, 7};
inline constexpr RegisterX64 r8{SizeX64::qword, 8};
inline constexpr RegisterX64 r9{SizeX64::qword, 9};
inline constexpr RegisterX64 r10{SizeX64::qword, 10};
inline constexpr RegisterX64 r11{SizeX64::qword, 11};
inline constexpr RegisterX64 r12{SizeX64::qword, 12};
inline constexpr RegisterX64 r13{SizeX64::qword, 13};
inline constexpr RegisterX64 r14{SizeX64::qword, 14};
inline constexpr RegisterX64 r15{SizeX64::qword, 15};

inline constexpr RegisterX64 xmm0{SizeX64::xmmword, 0};
inline constexpr RegisterX64 xmm1{SizeX64::xmmword, 1};
inline constexpr RegisterX64 xmm2{SizeX64::xmmword, 2};
inline constexpr RegisterX64 xmm3{SizeX64::xmmword, 3};
inline constexpr RegisterX64 xmm4{SizeX64::xmmword, 4};
inline constexpr RegisterX64 xmm5{SizeX64::xmmword, 5};
inline constexpr RegisterX64 xmm6{SizeX64::xmmword, 6};
inline constexpr RegisterX64 xmm7{SizeX64::xmmword, 7};
inline constexpr RegisterX64 xmm8{SizeX64::xmmword, 8};
inline constexpr RegisterX64 xmm9{SizeX64::xmmword, 9};
inline constexpr RegisterX64 xmm10{SizeX64::xmmword, 10};
inline constexpr RegisterX64 xmm11{SizeX64::xmmword, 11};
inline constexpr RegisterX64 xmm12{SizeX64::xmmword, 12};
inline constexpr RegisterX64 xmm13{SizeX64::xmmword, 13};
inline constexpr RegisterX64 xmm14{SizeX64::xmmword, 14};
inline constexpr RegisterX64 xmm15{SizeX64::xmmword, 15};

inline constexpr RegisterX64 ymm0{SizeX64::ymmword, 0};
inline constexpr RegisterX64 ymm1{SizeX64::ymmword, 1};
inline constexpr RegisterX64 ymm2{SizeX64::ymmword, 2};
inline constexpr RegisterX64 ymm3{SizeX64::ymmword, 3};
inline constexpr RegisterX64 ymm4{SizeX64::ymmword, 4};
inline constexpr RegisterX64 ymm5{SizeX64::ymmword, 5};
inline constexpr RegisterX64 ymm6{SizeX64::ymmword, 6};
inline constexpr RegisterX64 ymm7{SizeX64::ymmword, 7};
inline constexpr RegisterX64 ymm8{SizeX64::ymmword, 8};
inline constexpr RegisterX64 ymm9{SizeX64::ymmword, 9};
inline constexpr RegisterX64 ymm10{SizeX64::ymmword, 10};
inline constexpr RegisterX64 ymm11{SizeX64::ymmword, 11};
inline constexpr RegisterX64 ymm12{SizeX64::ymmword, 12};
inline constexpr RegisterX64 ymm13{SizeX64::ymmword, 13};
inline constexpr RegisterX64 ymm14{SizeX64::ymmword, 14};
inline constexpr RegisterX64 ymm15{SizeX64::ymmword, 15};

constexpr RegisterX64 byteReg(RegisterX64 reg)
{
    return RegisterX64{SizeX64::byte, reg.index};
}

constexpr RegisterX64 wordReg(RegisterX64 reg)
{
    return RegisterX64{SizeX64::word, reg.index};
}

constexpr RegisterX64 dwordReg(RegisterX64 reg)
{
    return RegisterX64{SizeX64::dword, reg.index};
}

constexpr RegisterX64 qwordReg(RegisterX64 reg)
{
    return RegisterX64{SizeX64::qword, reg.index};
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
