// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeGenCommon.h"

namespace Luau
{
namespace CodeGen
{

enum class ConditionX64 : uint8_t
{
    Overflow,
    NoOverflow,

    Carry,
    NoCarry,

    Below,
    BelowEqual,
    Above,
    AboveEqual,
    Equal,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    NotBelow,
    NotBelowEqual,
    NotAbove,
    NotAboveEqual,
    NotEqual,
    NotLess,
    NotLessEqual,
    NotGreater,
    NotGreaterEqual,

    Zero,
    NotZero,

    Parity,
    NotParity,

    Count
};

// Returns a condition that for 'a op b' will result in '!(a op b)'
inline ConditionX64 getNegatedCondition(ConditionX64 cond)
{
    switch (cond)
    {
    case ConditionX64::Overflow:
        return ConditionX64::NoOverflow;
    case ConditionX64::NoOverflow:
        return ConditionX64::Overflow;
    case ConditionX64::Carry:
        return ConditionX64::NoCarry;
    case ConditionX64::NoCarry:
        return ConditionX64::Carry;
    case ConditionX64::Below:
        return ConditionX64::NotBelow;
    case ConditionX64::BelowEqual:
        return ConditionX64::NotBelowEqual;
    case ConditionX64::Above:
        return ConditionX64::NotAbove;
    case ConditionX64::AboveEqual:
        return ConditionX64::NotAboveEqual;
    case ConditionX64::Equal:
        return ConditionX64::NotEqual;
    case ConditionX64::Less:
        return ConditionX64::NotLess;
    case ConditionX64::LessEqual:
        return ConditionX64::NotLessEqual;
    case ConditionX64::Greater:
        return ConditionX64::NotGreater;
    case ConditionX64::GreaterEqual:
        return ConditionX64::NotGreaterEqual;
    case ConditionX64::NotBelow:
        return ConditionX64::Below;
    case ConditionX64::NotBelowEqual:
        return ConditionX64::BelowEqual;
    case ConditionX64::NotAbove:
        return ConditionX64::Above;
    case ConditionX64::NotAboveEqual:
        return ConditionX64::AboveEqual;
    case ConditionX64::NotEqual:
        return ConditionX64::Equal;
    case ConditionX64::NotLess:
        return ConditionX64::Less;
    case ConditionX64::NotLessEqual:
        return ConditionX64::LessEqual;
    case ConditionX64::NotGreater:
        return ConditionX64::Greater;
    case ConditionX64::NotGreaterEqual:
        return ConditionX64::GreaterEqual;
    case ConditionX64::Zero:
        return ConditionX64::NotZero;
    case ConditionX64::NotZero:
        return ConditionX64::Zero;
    case ConditionX64::Parity:
        return ConditionX64::NotParity;
    case ConditionX64::NotParity:
        return ConditionX64::Parity;
    case ConditionX64::Count:
        CODEGEN_ASSERT(!"invalid ConditionX64 value");
    }

    return ConditionX64::Count;
}

// Returns a condition that for 'a op b' will result in 'b op a'
// Only a subset of conditions is allowed
inline ConditionX64 getInverseCondition(ConditionX64 cond)
{
    switch (cond)
    {
    case ConditionX64::Below:
        return ConditionX64::Above;
    case ConditionX64::BelowEqual:
        return ConditionX64::AboveEqual;
    case ConditionX64::Above:
        return ConditionX64::Below;
    case ConditionX64::AboveEqual:
        return ConditionX64::BelowEqual;
    case ConditionX64::Equal:
        return ConditionX64::Equal;
    case ConditionX64::Less:
        return ConditionX64::Greater;
    case ConditionX64::LessEqual:
        return ConditionX64::GreaterEqual;
    case ConditionX64::Greater:
        return ConditionX64::Less;
    case ConditionX64::GreaterEqual:
        return ConditionX64::LessEqual;
    case ConditionX64::NotBelow:
        return ConditionX64::NotAbove;
    case ConditionX64::NotBelowEqual:
        return ConditionX64::NotAboveEqual;
    case ConditionX64::NotAbove:
        return ConditionX64::NotBelow;
    case ConditionX64::NotAboveEqual:
        return ConditionX64::NotBelowEqual;
    case ConditionX64::NotEqual:
        return ConditionX64::NotEqual;
    case ConditionX64::NotLess:
        return ConditionX64::NotGreater;
    case ConditionX64::NotLessEqual:
        return ConditionX64::NotGreaterEqual;
    case ConditionX64::NotGreater:
        return ConditionX64::NotLess;
    case ConditionX64::NotGreaterEqual:
        return ConditionX64::NotLessEqual;
    default:
        CODEGEN_ASSERT(!"invalid ConditionX64 value for getInverseCondition");
    }

    return ConditionX64::Count;
}

} // namespace CodeGen
} // namespace Luau
