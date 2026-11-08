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

#ifndef DM_JSON_NUMBER_H
#define DM_JSON_NUMBER_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "json_number_tables.h"

namespace dmJson
{
    // Independently implemented interval conversion. The generated table holds
    // floor(10^q / 2^b), normalized to 128 bits. Multiplying its lower and upper
    // bounds by the decimal significand encloses the exact value. If both bounds
    // round to the same binary64, that is also the correctly rounded answer.

    static uint32_t BitLength(uint64_t value)
    {
        if (!value)
        {
            return 0;
        }
#if defined(__GNUC__) || defined(__clang__)
        return 64 - __builtin_clzll(value);
#else
        uint32_t bits = 0;
        while (value)
        {
            ++bits;
            value >>= 1;
        }
        return bits;
#endif
    }

    static void Multiply64(uint64_t a, uint64_t b, uint64_t* low, uint64_t* high)
    {
#if defined(__SIZEOF_INT128__) && !defined(DM_JSON_FORCE_PORTABLE_MULTIPLY)
        __uint128_t product = (__uint128_t)a * b;
        *low = (uint64_t)product;
        *high = (uint64_t)(product >> 64);
#else
        uint64_t p00 = (uint64_t)(uint32_t)a * (uint32_t)b;
        uint64_t p01 = (uint64_t)(uint32_t)a * (b >> 32);
        uint64_t p10 = (a >> 32) * (uint32_t)b;
        uint64_t p11 = (a >> 32) * (b >> 32);
        uint64_t middle = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
        *low = (middle << 32) | (uint32_t)p00;
        *high = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
#endif
    }

    static uint64_t BinaryBits(uint64_t significand, int exponent)
    {
        if (significand == (UINT64_C(1) << 53))
        {
            significand >>= 1;
            ++exponent;
        }
        if (exponent > 1023)
        {
            return UINT64_C(0x7ff0000000000000);
        }
        if (exponent < -1022)
        {
            return significand;
        }
        return ((uint64_t)(exponent + 1023) << 52) | (significand & UINT64_C(0xfffffffffffff));
    }

    static uint64_t RoundProduct(const uint64_t words[3], int power)
    {
        int bits = words[2] ? 128 + BitLength(words[2]) : 64 + BitLength(words[1]);
        int exponent = bits - 1 + power;
        if (exponent > 1023)
        {
            return UINT64_C(0x7ff0000000000000);
        }
        if (exponent < -1075)
        {
            return 0;
        }
        int quantum = exponent < -1022 ? -1074 : exponent - 52;
        uint32_t shift = (uint32_t)(quantum - power);
        uint32_t word = shift / 64;
        uint32_t offset = shift % 64;
        uint64_t significand = word < 3 ? words[word] >> offset : 0;
        if (offset && word < 2)
        {
            significand |= words[word + 1] << (64 - offset);
        }
        uint32_t half_word = (shift - 1) / 64;
        uint32_t half_bit = (shift - 1) % 64;
        bool half = half_word < 3 && ((words[half_word] >> half_bit) & 1);
        bool sticky = false;
        for (uint32_t i = 0; i < half_word && i < 3; ++i)
        {
            sticky |= words[i] != 0;
        }
        if (half_word < 3 && half_bit)
        {
            sticky |= (words[half_word] & ((UINT64_C(1) << half_bit) - 1)) != 0;
        }
        significand += half && (sticky || (significand & 1));
        return BinaryBits(significand, exponent);
    }

    static bool TableDecimal(uint64_t significand, int64_t exponent, uint64_t* bits)
    {
        if (exponent < -342 || exponent > 308 || !significand)
        {
            return false;
        }
        const DecimalPower& power = DECIMAL_POWERS[exponent + 342];
        uint64_t words[3];
        uint64_t carry;
        Multiply64(significand, power.m_Low, &words[0], &carry);
        Multiply64(significand, power.m_High, &words[1], &words[2]);
        words[1] += carry;
        words[2] += words[1] < carry;
        *bits = RoundProduct(words, power.m_Exponent);
        if (power.m_Exact)
        {
            return true;
        }
        uint64_t old = words[0];
        words[0] += significand;
        if (words[0] < old && ++words[1] == 0)
        {
            ++words[2];
        }
        return *bits == RoundProduct(words, power.m_Exponent);
    }

    // The exact fallback keeps 800 significant decimal digits plus a sticky bit.
    // Every binary64 rounding boundary has at most 768 significant decimal
    // digits: its denominator divides 2^1075 and its numerator has <=54 bits.
    // Discarded digits can therefore only break an exact tie, never cross one.
    // After decimal range checks the largest integer below needs <2700 bits.
    struct DecimalBig
    {
        uint32_t m_Words[96];
        uint32_t m_Size;
    };

    static void BigMultiplyAdd(DecimalBig* value, uint32_t multiplier, uint32_t add)
    {
        uint64_t carry = add;
        for (uint32_t i = 0; i < value->m_Size; ++i)
        {
            carry += (uint64_t)value->m_Words[i] * multiplier;
            value->m_Words[i] = (uint32_t)carry;
            carry >>= 32;
        }
        if (carry)
        {
            assert(value->m_Size < 96);
            value->m_Words[value->m_Size++] = (uint32_t)carry;
        }
    }

    static int BigBits(const DecimalBig& value)
    {
        return value.m_Size ? (value.m_Size - 1) * 32 + BitLength(value.m_Words[value.m_Size - 1]) : 0;
    }

    static void BigShift(DecimalBig* value, uint32_t shift)
    {
        uint32_t whole = shift / 32;
        uint32_t part = shift % 32;
        uint32_t size = value->m_Size;
        assert(size + whole + 1 <= 96);
        if (part)
        {
            uint32_t carry = 0;
            for (uint32_t i = 0; i < size; ++i)
            {
                uint64_t word = ((uint64_t)value->m_Words[i] << part) | carry;
                value->m_Words[i] = (uint32_t)word;
                carry = (uint32_t)(word >> 32);
            }
            if (carry)
            {
                value->m_Words[size++] = carry;
            }
        }
        if (whole)
        {
            memmove(value->m_Words + whole, value->m_Words, size * sizeof(uint32_t));
            memset(value->m_Words, 0, whole * sizeof(uint32_t));
        }
        value->m_Size = size + whole;
    }

    static int BigCompare(const DecimalBig& a, const DecimalBig& b)
    {
        if (a.m_Size != b.m_Size)
        {
            return a.m_Size < b.m_Size ? -1 : 1;
        }
        for (uint32_t i = a.m_Size; i > 0; --i)
        {
            if (a.m_Words[i - 1] != b.m_Words[i - 1])
            {
                return a.m_Words[i - 1] < b.m_Words[i - 1] ? -1 : 1;
            }
        }
        return 0;
    }

    static void BigSubtract(DecimalBig* a, const DecimalBig& b)
    {
        uint64_t borrow = 0;
        for (uint32_t i = 0; i < a->m_Size; ++i)
        {
            uint64_t sub = (i < b.m_Size ? b.m_Words[i] : 0) + borrow;
            borrow = a->m_Words[i] < sub;
            a->m_Words[i] -= (uint32_t)sub;
        }
        while (a->m_Size && !a->m_Words[a->m_Size - 1])
        {
            --a->m_Size;
        }
    }

    static void BigHalve(DecimalBig* value)
    {
        uint32_t carry = 0;
        for (uint32_t i = value->m_Size; i > 0; --i)
        {
            uint32_t word = value->m_Words[i - 1];
            value->m_Words[i - 1] = (word >> 1) | carry;
            carry = word << 31;
        }
        if (value->m_Size && !value->m_Words[value->m_Size - 1])
        {
            --value->m_Size;
        }
    }

    static uint64_t ExactDecimal(const char* begin, const char* end, int64_t exponent)
    {
        DecimalBig numerator;
        numerator.m_Size = 0;
        uint32_t digits = 0;
        bool sticky = false;
        for (const char* p = begin + (*begin == '-'); p < end && *p != 'e' && *p != 'E'; ++p)
        {
            if (*p == '.')
            {
                continue;
            }
            uint32_t digit = (uint32_t)(*p - '0');
            if (!digits && !digit)
            {
                continue;
            }
            if (digits < 800)
            {
                BigMultiplyAdd(&numerator, 10, digit);
            }
            else
            {
                sticky |= digit != 0;
            }
            ++digits;
        }
        if (!digits || exponent + digits - 1 < -324)
        {
            return 0;
        }
        if (exponent + digits - 1 > 308)
        {
            return UINT64_C(0x7ff0000000000000);
        }
        if (digits > 800)
        {
            exponent += digits - 800;
        }
        DecimalBig denominator;
        denominator.m_Size = 1;
        denominator.m_Words[0] = 1;
        DecimalBig* scaled = exponent >= 0 ? &numerator : &denominator;
        int powers = (int)(exponent >= 0 ? exponent : -exponent);
        while (powers >= 13)
        {
            BigMultiplyAdd(scaled, 1220703125, 0);
            powers -= 13;
        }
        while (powers--)
        {
            BigMultiplyAdd(scaled, 5, 0);
        }
        int binary_exponent = BigBits(numerator) - BigBits(denominator);
        DecimalBig temporary = binary_exponent >= 0 ? denominator : numerator;
        BigShift(&temporary, binary_exponent >= 0 ? binary_exponent : -binary_exponent);
        if ((binary_exponent >= 0 ? BigCompare(numerator, temporary) : BigCompare(temporary, denominator)) < 0)
        {
            --binary_exponent;
        }
        binary_exponent += (int)exponent;
        int quantum = binary_exponent < -1022 ? -1074 : binary_exponent - 52;
        int shift = (int)exponent - quantum;
        BigShift(shift >= 0 ? &numerator : &denominator, shift >= 0 ? shift : -shift);
        int division_shift = BigBits(numerator) - BigBits(denominator);
        temporary = denominator;
        if (division_shift >= 0)
        {
            BigShift(&temporary, division_shift);
        }
        uint64_t significand = 0;
        for (int i = division_shift; i >= 0; --i)
        {
            if (BigCompare(numerator, temporary) >= 0)
            {
                BigSubtract(&numerator, temporary);
                significand |= UINT64_C(1) << i;
            }
            BigHalve(&temporary);
        }
        BigMultiplyAdd(&numerator, 2, 0);
        int comparison = BigCompare(numerator, denominator);
        significand += comparison > 0 || (comparison == 0 && (sticky || (significand & 1)));
        return BinaryBits(significand, binary_exponent);
    }

    // The lexer has validated [begin, end) and accumulated its significand and
    // decimal exponent. Conversion assumes IEEE-754 binary64 round-to-nearest,
    // ties-to-even, and never consults or changes the process locale.
    static double ReadDecimal(const char* begin, const char* end, uint64_t coefficient,
                              uint32_t digits, int64_t exponent)
    {
        static const double powers[] = {
            1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
            1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
        };
        bool negative = *begin == '-';
        if (digits <= 19)
        {
            if (!coefficient || exponent == 0)
            {
                double value = (double)coefficient;
                return negative ? -value : value;
            }
            // Both operands are exact. One IEEE-754 multiply or divide provides
            // the required rounding (round-to-nearest, ties-to-even).
            if (coefficient <= (UINT64_C(1) << 53) && exponent >= -22 && exponent <= 22)
            {
                double value = exponent < 0 ? (double)coefficient / powers[-exponent] : (double)coefficient * powers[exponent];
                return negative ? -value : value;
            }
        }
        uint64_t bits;
        if (digits > 19 || !TableDecimal(coefficient, exponent, &bits))
        {
            // A long significand lies between its first 19 digits and that
            // prefix plus one, scaled by the discarded decimal places. Most
            // such intervals round identically; exact halfway cases fall back.
            if (digits > 19)
            {
                uint64_t prefix = 0;
                uint32_t kept = 0;
                for (const char* p = begin + negative; kept < 19; ++p)
                {
                    if (*p != '.')
                    {
                        prefix = prefix * 10 + (uint32_t)(*p - '0');
                        ++kept;
                    }
                }
                uint64_t upper;
                int64_t adjusted = exponent + digits - 19;
                if (TableDecimal(prefix, adjusted, &bits) && TableDecimal(prefix + 1, adjusted, &upper) && bits == upper)
                {
                    bits |= (uint64_t)negative << 63;
                    double value;
                    memcpy(&value, &bits, sizeof(value));
                    return value;
                }
            }
            bits = ExactDecimal(begin, end, exponent);
        }
        bits |= (uint64_t)negative << 63;
        double value;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }

}
#endif
