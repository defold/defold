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

#include "json_number.h"
#include <dlib/inline.h>
#include <math.h>

namespace dmJson
{
    namespace
    {
        // Emit two digits per division after the coefficient has been rounded.
        static const char DIGIT_PAIRS[] =
            "00010203040506070809"
            "10111213141516171819"
            "20212223242526272829"
            "30313233343536373839"
            "40414243444546474849"
            "50515253545556575859"
            "60616263646566676869"
            "70717273747576777879"
            "80818283848586878889"
            "90919293949596979899";

        // Subnormal formatting can require powers beyond the reader's cache.
        // Normalized high words for 10^309..10^337; generated with
        // engine/dlib/scripts/gen_json_number_tables.py --encode.
        static const uint64_t ENCODE_POWERS_HIGH[] = {
            UINT64_C(0xb201833b35d63f73), // 10^309
            UINT64_C(0xde81e40a034bcf4f), // 10^310
            UINT64_C(0x8b112e86420f6191), // 10^311
            UINT64_C(0xadd57a27d29339f6), // 10^312
            UINT64_C(0xd94ad8b1c7380874), // 10^313
            UINT64_C(0x87cec76f1c830548), // 10^314
            UINT64_C(0xa9c2794ae3a3c69a), // 10^315
            UINT64_C(0xd433179d9c8cb841), // 10^316
            UINT64_C(0x849feec281d7f328), // 10^317
            UINT64_C(0xa5c7ea73224deff3), // 10^318
            UINT64_C(0xcf39e50feae16bef), // 10^319
            UINT64_C(0x81842f29f2cce375), // 10^320
            UINT64_C(0xa1e53af46f801c53), // 10^321
            UINT64_C(0xca5e89b18b602368), // 10^322
            UINT64_C(0xfcf62c1dee382c42), // 10^323
            UINT64_C(0x9e19db92b4e31ba9), // 10^324
            UINT64_C(0xc5a05277621be293), // 10^325
            UINT64_C(0xf70867153aa2db38), // 10^326
            UINT64_C(0x9a65406d44a5c903), // 10^327
            UINT64_C(0xc0fe908895cf3b44), // 10^328
            UINT64_C(0xf13e34aabb430a15), // 10^329
            UINT64_C(0x96c6e0eab509e64d), // 10^330
            UINT64_C(0xbc789925624c5fe0), // 10^331
            UINT64_C(0xeb96bf6ebadf77d8), // 10^332
            UINT64_C(0x933e37a534cbaae7), // 10^333
            UINT64_C(0xb80dc58e81fe95a1), // 10^334
            UINT64_C(0xe61136f2227e3b09), // 10^335
            UINT64_C(0x8fcac257558ee4e6), // 10^336
            UINT64_C(0xb3bd72ed2af29e1f), // 10^337
        };

        // Seed the shared bounded integer representation without clearing unused words.
        static void SetDecimalBig(DecimalBig* value, uint64_t number)
        {
            value->m_Words[0] = (uint32_t)number;
            value->m_Words[1] = (uint32_t)(number >> 32);
            value->m_Size = value->m_Words[1] ? 2 : 1;
        }

        // Compare the exact scaled value with lower + 0.5. Only values close to
        // a rounding midpoint need this bounded integer work; no heap is used.
        static uint64_t RoundDecimalExact(uint64_t significand, int binary_exponent, int power, uint64_t lower)
        {
            DecimalBig numerator, denominator;
            SetDecimalBig(&numerator, significand);
            SetDecimalBig(&denominator, lower * 2 + 1);
            DecimalBig* scaled = power >= 0 ? &numerator : &denominator;
            int powers = power >= 0 ? power : -power;
            // Multiply by 5^13 per step to keep the uncommon exact check short.
            while (powers >= 13)
            {
                BigMultiplyAdd(scaled, 1220703125, 0);
                powers -= 13;
            }
            while (powers--)
            {
                BigMultiplyAdd(scaled, 5, 0);
            }
            // 2 * value * 10^power = significand * 5^power * 2^(e2+power+1).
            // Negative powers move the corresponding factors to the other side.
            int shift = binary_exponent + power + 1;
            BigShift(shift >= 0 ? &numerator : &denominator, shift >= 0 ? shift : -shift);
            int comparison = BigCompare(numerator, denominator);
            return lower + (comparison > 0 || (comparison == 0 && (lower & 1)));
        }

        // Round significand * 2^binary_exponent * 10^power to the nearest even
        // integer. Omitting the cache's low word bounds the normalized product
        // between high and high+2. Away from a midpoint both bounds round alike.
        static DM_FORCE_INLINE uint64_t RoundDecimal(uint64_t significand, int binary_exponent, int power, uint32_t leading)
        {
            uint64_t cached = power <= 308 ? DECIMAL_POWERS[power + 342].m_High : ENCODE_POWERS_HIGH[power - 309];
            int cache_exponent = ((power * 217706 + 134217728) >> 16) - 2175;
            uint64_t normalized = significand << leading;
            uint64_t ignored, high;
            Multiply64(normalized, cached, &ignored, &high);
            int shift = (int)leading - binary_exponent - cache_exponent - 128;
            uint64_t halfway = UINT64_C(1) << (shift - 1);
            uint64_t remainder = high & ((UINT64_C(1) << shift) - 1);
            uint64_t integer = high >> shift;
            if (power >= 0 && power <= 27)
            {
                // 5^27 fits in 64 bits, so these cached high words are exact.
                // Only an exact high-word tie needs the low product and parity.
                return integer + (remainder > halfway ||
                                  (remainder == halfway && (normalized * cached != 0 || (integer & 1))));
            }
            if (remainder < halfway - 2 || remainder > halfway)
            {
                return integer + (remainder > halfway);
            }
            return RoundDecimalExact(significand, binary_exponent, power, integer);
        }

        // Render the rounded coefficient with the existing %g notation rules.
        static DM_FORCE_INLINE uint32_t WriteDecimal(uint64_t coefficient, int exponent, bool negative, char* text)
        {
            char digits[14];
            for (int i = 12; i >= 0; i -= 2)
            {
                uint32_t pair = (uint32_t)(coefficient % 100);
                coefficient /= 100;
                memcpy(digits + i, DIGIT_PAIRS + pair * 2, 2);
            }
            int length = 14;
            while (length > 1 && digits[length - 1] == '0')
            {
                --length;
            }
            // Sign, fourteen digits, point and e+/-ddd fit in the output span.
            char* out = text;
            if (negative)
            {
                *out++ = '-';
            }
            if (exponent < -4 || exponent >= 14)
            {
                *out++ = digits[0];
                if (length > 1)
                {
                    *out++ = '.';
                    memcpy(out, digits + 1, length - 1);
                    out += length - 1;
                }
                *out++ = 'e';
                *out++ = exponent < 0 ? '-' : '+';
                uint32_t power = (uint32_t)(exponent < 0 ? -exponent : exponent);
                if (power >= 100)
                {
                    *out++ = '0' + power / 100;
                    power %= 100;
                }
                memcpy(out, DIGIT_PAIRS + power * 2, 2);
                out += 2;
            }
            else
            {
                int point = exponent + 1;
                if (point <= 0)
                {
                    *out++ = '0';
                    *out++ = '.';
                    while (point++ < 0)
                    {
                        *out++ = '0';
                    }
                    memcpy(out, digits, length);
                    out += length;
                }
                else
                {
                    for (int i = 0; i < length || i < point; ++i)
                    {
                        if (i == point)
                        {
                            *out++ = '.';
                        }
                        *out++ = i < length ? digits[i] : '0';
                    }
                }
            }
            return (uint32_t)(out - text);
        }

        // All multipliers are exact integer powers of ten. Directed rounding
        // cannot cross a representable half-integer without landing on it.
        // Return false when the integer fallback is needed.
        static DM_FORCE_INLINE bool SmallDecimal(double value, char* text, uint32_t* size)
        {
            bool negative = value < 0;
            double magnitude = negative ? -value : value;
            if (magnitude < 1e-4 || magnitude >= 1e14)
            {
                return false;
            }
            static const double powers[] = {
                1e-5, 1e-4, 1e-3, 1e-2, 1e-1, 1e0, 1e1, 1e2, 1e3, 1e4,
                1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14,
                1e15, 1e16, 1e17
            };
            uint64_t bits;
            memcpy(&bits, &magnitude, sizeof(bits));
            int binary_exponent = (int)(bits >> 52) - 1023;
            // In this range, floor(e2 * log10(2)) needs at most one correction.
            // The bias keeps the integer shift nonnegative and portable.
            int exponent = ((binary_exponent * 1233 + 65536) >> 12) - 16;
            if (magnitude >= powers[exponent + 6])
            {
                ++exponent;
            }
            // These integer powers of ten are exact doubles, and the product
            // has at most 47 integer bits. Rounding cannot cross a half
            // integer in any rounding mode unless it lands on one. The
            // integer fallback resolves that ambiguity exactly.
            double scaled = magnitude * powers[18 - exponent];
            uint64_t coefficient = (uint64_t)scaled;
            double fraction = scaled - (double)coefficient;
            if (fraction == 0.5)
            {
                return false;
            }
            coefficient += fraction > 0.5;
            if (coefficient == UINT64_C(100000000000000))
            {
                coefficient /= 10;
                if (++exponent == 14)
                {
                    return false;
                }
            }
            *size = WriteDecimal(coefficient, exponent, negative, text);
            return true;
        }

        // Wide values and ambiguous common products use integer-only rounding.
        static uint32_t Decimal(uint64_t significand, int binary_exponent, bool negative, char* text)
        {
            uint32_t bits = BitLength(significand);
            uint32_t leading = 64 - bits;
            // floor(log10(2^e2)) is a lower bound on the decimal exponent.
            // The bias avoids shifting negative integers over the binary64 range.
            int floor_binary = binary_exponent + (int)bits - 1;
            int exponent = ((floor_binary * 78913 + 1073741824) >> 18) - 4096;
            uint64_t coefficient = RoundDecimal(significand, binary_exponent, 13 - exponent, leading);
            if (coefficient >= UINT64_C(100000000000000))
            {
                // Recompute at the final precision, avoiding double rounding
                // when the exponent estimate was one below the true exponent.
                ++exponent;
                coefficient = RoundDecimal(significand, binary_exponent, 13 - exponent, leading);
            }
            return WriteDecimal(coefficient, exponent, negative, text);
        }
    }

    // Keep common number paths together; only ambiguous midpoint checks
    // need the separate bounded integer helper.
    uint32_t WriteNumber(double value, char* text)
    {
        // Preserve the existing %.14g representation, including signed zero.
        if (value > -1e14 && value < 1e14 && floor(value) == value)
        {
            char digits[16];
            char* out = digits + sizeof(digits);
            bool negative = signbit(value) != 0;
            uint64_t integer = (uint64_t)(negative ? -value : value);
            // Write complete pairs backwards, then the one- or
            // two-digit leading group without adding a leading zero.
            while (integer >= 100)
            {
                uint32_t pair = (uint32_t)(integer % 100);
                integer /= 100;
                out -= 2;
                memcpy(out, DIGIT_PAIRS + pair * 2, 2);
            }
            if (integer < 10)
            {
                *--out = '0' + (uint32_t)integer;
            }
            else
            {
                out -= 2;
                memcpy(out, DIGIT_PAIRS + (uint32_t)integer * 2, 2);
            }
            if (negative)
            {
                *--out = '-';
            }
            uint32_t size = (uint32_t)(digits + sizeof(digits) - out);
            memcpy(text, out, size);
            return size;
        }
        uint32_t size;
        if (SmallDecimal(value, text, &size))
        {
            return size;
        }
        uint64_t raw;
        memcpy(&raw, &value, sizeof(raw));
        uint32_t exponent = (uint32_t)(raw >> 52) & 2047;
        uint64_t significand = raw & UINT64_C(0xfffffffffffff);
        if (exponent)
        {
            significand |= UINT64_C(1) << 52;
        }
        return Decimal(significand, exponent ? (int)exponent - 1075 : -1074, (raw >> 63) != 0, text);
    }
}
