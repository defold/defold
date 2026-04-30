// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/RegisterA64.h"

#include <stddef.h>

namespace Luau
{
namespace CodeGen
{
namespace A64
{

enum class AddressKindA64 : uint8_t
{
    reg,  // reg + reg
    imm,  // reg + imm
    pre,  // reg + imm, reg += imm
    post, // reg, reg += imm
};

struct AddressA64
{
    // This is a little misleading since AddressA64 can encode offsets up to 1023*size where size depends on the load/store size
    // For example, ldr x0, [reg+imm] is limited to 8 KB offsets assuming imm is divisible by 8, but loading into w0 reduces the range to 4 KB
    static constexpr size_t kMaxOffset = 1023;

    constexpr AddressA64(RegisterA64 base, int off = 0, AddressKindA64 kind = AddressKindA64::imm)
        : kind(kind)
        , base(base)
        , offset(xzr)
        , data(off)
    {
        CODEGEN_ASSERT(base.kind == KindA64::x || base == sp);
        CODEGEN_ASSERT(kind != AddressKindA64::reg);
    }

    constexpr AddressA64(RegisterA64 base, RegisterA64 offset)
        : kind(AddressKindA64::reg)
        , base(base)
        , offset(offset)
        , data(0)
    {
        CODEGEN_ASSERT(base.kind == KindA64::x);
        CODEGEN_ASSERT(offset.kind == KindA64::x);
    }

    AddressKindA64 kind;
    RegisterA64 base;
    RegisterA64 offset;
    int data;
};

using mem = AddressA64;

} // namespace A64
} // namespace CodeGen
} // namespace Luau
