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

#include <dlib/inline.h>
#include <dlib/json_lexer.h>
#include "script_json.h"
#include <limits.h>
#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmScript
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
        UINT64_C(0xb201833b35d63f73),     // 10^309
        UINT64_C(0xde81e40a034bcf4f),     // 10^310
        UINT64_C(0x8b112e86420f6191),     // 10^311
        UINT64_C(0xadd57a27d29339f6),     // 10^312
        UINT64_C(0xd94ad8b1c7380874),     // 10^313
        UINT64_C(0x87cec76f1c830548),     // 10^314
        UINT64_C(0xa9c2794ae3a3c69a),     // 10^315
        UINT64_C(0xd433179d9c8cb841),     // 10^316
        UINT64_C(0x849feec281d7f328),     // 10^317
        UINT64_C(0xa5c7ea73224deff3),     // 10^318
        UINT64_C(0xcf39e50feae16bef),     // 10^319
        UINT64_C(0x81842f29f2cce375),     // 10^320
        UINT64_C(0xa1e53af46f801c53),     // 10^321
        UINT64_C(0xca5e89b18b602368),     // 10^322
        UINT64_C(0xfcf62c1dee382c42),     // 10^323
        UINT64_C(0x9e19db92b4e31ba9),     // 10^324
        UINT64_C(0xc5a05277621be293),     // 10^325
        UINT64_C(0xf70867153aa2db38),     // 10^326
        UINT64_C(0x9a65406d44a5c903),     // 10^327
        UINT64_C(0xc0fe908895cf3b44),     // 10^328
        UINT64_C(0xf13e34aabb430a15),     // 10^329
        UINT64_C(0x96c6e0eab509e64d),     // 10^330
        UINT64_C(0xbc789925624c5fe0),     // 10^331
        UINT64_C(0xeb96bf6ebadf77d8),     // 10^332
        UINT64_C(0x933e37a534cbaae7),     // 10^333
        UINT64_C(0xb80dc58e81fe95a1),     // 10^334
        UINT64_C(0xe61136f2227e3b09),     // 10^335
        UINT64_C(0x8fcac257558ee4e6),     // 10^336
        UINT64_C(0xb3bd72ed2af29e1f),     // 10^337
    };

    // Seed the shared bounded integer representation without clearing unused words.
    static void SetDecimalBig(dmJson::DecimalBig* value, uint64_t number)
    {
        value->m_Words[0] = (uint32_t)number;
        value->m_Words[1] = (uint32_t)(number >> 32);
        value->m_Size = value->m_Words[1] ? 2 : 1;
    }

    // Compare the exact scaled value with lower + 0.5. Only values close to
    // a rounding midpoint need this bounded integer work; no heap is used.
    static uint64_t RoundDecimalExact(uint64_t significand, int binary_exponent, int power, uint64_t lower)
    {
        dmJson::DecimalBig numerator, denominator;
        SetDecimalBig(&numerator, significand);
        SetDecimalBig(&denominator, lower * 2 + 1);
        dmJson::DecimalBig* scaled = power >= 0 ? &numerator : &denominator;
        int powers = power >= 0 ? power : -power;
        // Multiply by 5^13 per step to keep the uncommon exact check short.
        while (powers >= 13)
        {
            dmJson::BigMultiplyAdd(scaled, 1220703125, 0);
            powers -= 13;
        }
        while (powers--)
        {
            dmJson::BigMultiplyAdd(scaled, 5, 0);
        }
        // 2 * value * 10^power = significand * 5^power * 2^(e2+power+1).
        // Negative powers move the corresponding factors to the other side.
        int shift = binary_exponent + power + 1;
        dmJson::BigShift(shift >= 0 ? &numerator : &denominator, shift >= 0 ? shift : -shift);
        int comparison = dmJson::BigCompare(numerator, denominator);
        return lower + (comparison > 0 || (comparison == 0 && (lower & 1)));
    }

    // Round significand * 2^binary_exponent * 10^power to the nearest even
    // integer. Omitting the cache's low word bounds the normalized product
    // between high and high+2. Away from a midpoint both bounds round alike.
    static DM_FORCE_INLINE uint64_t RoundDecimal(uint64_t significand, int binary_exponent, int power, uint32_t leading)
    {
        uint64_t cached = power <= 308 ? dmJson::GetDecimalPowers()[power + 342].m_High : ENCODE_POWERS_HIGH[power - 309];
        int cache_exponent = ((power * 217706 + 134217728) >> 16) - 2175;
        uint64_t normalized = significand << leading;
        uint64_t ignored, high;
        dmJson::Multiply64(normalized, cached, &ignored, &high);
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

    struct JsonWriter
    {
        dmJson::StringBuffer m_Buffer;
        uint32_t             m_Size;
        lua_State*           m_Lua;
        bool                 m_EmptyObject;
        JsonWriter(bool empty_object) : m_Size(0), m_Lua(0), m_EmptyObject(empty_object)
        {
        }
        void Reserve(uint32_t extra)
        {
            if (extra >= UINT32_MAX - m_Size || !m_Buffer.Reserve(m_Size + extra + 1))
            {
                luaL_error(m_Lua, "Unable to allocate JSON output buffer");
            }
        }
        void Append(const char* text, uint32_t size)
        {
            Reserve(size);
            memcpy(m_Buffer.m_Data + m_Size, text, size);
            m_Size += size;
        }
        void Byte(char c)
        {
            Reserve(1);
            m_Buffer.m_Data[m_Size++] = c;
        }
        static bool PlainBlock(const char* text)
        {
            if (!dmJson::PlainStringBlock(text, false))
            {
                return false;
            }
            dsimd_uint4_t slash = SimdXorUint(SimdLoadUint(text), SimdSplatUint(0x2f2f2f2f));
            dsimd_uint4_t found = SimdAndUint(SimdSubUint(slash, SimdSplatUint(0x01010101)), SimdNotUint(slash));
            dsimd_uint4_t del = SimdXorUint(SimdLoadUint(text), SimdSplatUint(0x7f7f7f7f));
            found = SimdOrUint(found, SimdAndUint(SimdSubUint(del, SimdSplatUint(0x01010101)), SimdNotUint(del)));
            return SimdAll(SimdEqualUint(SimdAndUint(found, SimdSplatUint(0x80808080)), SimdSplatUint(0)));
        }
        void String(int index)
        {
            size_t length;
            const char* text = lua_tolstring(m_Lua, index, &length);
            if (length >= UINT32_MAX - 2)
            {
                luaL_error(m_Lua, "JSON string exceeds the addressable size");
            }
            uint32_t size = (uint32_t)length;
            Reserve(size + 2);
            Byte('"');
            // Low byte: output length; high byte: the character after a short escape.
            static const uint16_t escapes[256] = {
                6, 6, 6, 6, 6, 6, 6, 6, 25090, 29698, 28162, 6, 26114, 29186, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                0, 0, 8706, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12034,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 23554, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            };
            uint32_t i = 0;
            while (i < size)
            {
                uint32_t remaining = size - i;
                Reserve((remaining < 32 ? remaining : 32) * 6 + 2);
                // Each input byte expands to at most six output bytes. Check
                // the buffer once per bounded run, then write into it directly.
                uint32_t count = (m_Buffer.m_Capacity - m_Size - 2) / 6;
                uint32_t end = i + (count < remaining ? count : remaining);
                char* out = m_Buffer.m_Data + m_Size;
                uint32_t plain = 0;
                while (i < end)
                {
                    unsigned char c = text[i++];
                    uint16_t escape = escapes[c];
                    if (!escape)
                    {
                        *out++ = c;
                        if (++plain == 8)
                        {
                            while (end - i >= 16 && PlainBlock(text + i))
                            {
                                memcpy(out, text + i, 16);
                                out += 16;
                                i += 16;
                            }
                            plain = 0;
                        }
                        continue;
                    }
                    plain = 0;
                    *out++ = '\\';
                    if ((escape & 255) == 2)
                    {
                        *out++ = escape >> 8;
                    }
                    else
                    {
                        static const char hex[] = "0123456789abcdef";
                        *out++ = 'u';
                        *out++ = '0';
                        *out++ = '0';
                        *out++ = hex[c >> 4];
                        *out++ = hex[c & 15];
                    }
                }
                m_Size = (uint32_t)(out - m_Buffer.m_Data);
            }
            Byte('"');
        }
        // Render the rounded coefficient with the existing %g notation rules.
        DM_FORCE_INLINE void WriteDecimal(uint64_t coefficient, int exponent, bool negative)
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
            // Sign, fourteen digits, point and e+/-ddd fit in 24 bytes.
            Reserve(24);
            char* out = m_Buffer.m_Data + m_Size;
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
            m_Size = (uint32_t)(out - m_Buffer.m_Data);
        }
        // All multipliers are exact integer powers of ten. Directed rounding
        // cannot cross a representable half-integer without landing on it.
        DM_FORCE_INLINE bool SmallDecimal(double value)
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
            WriteDecimal(coefficient, exponent, negative);
            return true;
        }
        // Wide values and ambiguous common products use integer-only rounding.
        void Decimal(uint64_t significand, int binary_exponent, bool negative)
        {
            uint32_t bits = dmJson::BitLength(significand);
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
            WriteDecimal(coefficient, exponent, negative);
        }
        // Keep formatting in the caller's loop. Avoiding a call per numeric
        // value gives a small improvement on Wasm.
        DM_FORCE_INLINE void Number(int index)
        {
            double value = lua_tonumber(m_Lua, index);
            if (isnan(value) || isinf(value))
            {
                luaL_error(m_Lua, "Cannot serialise number: must not be NaN or Infinity");
            }
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
                Append(out, (uint32_t)(digits + sizeof(digits) - out));
                return;
            }
            if (SmallDecimal(value))
            {
                return;
            }
            uint64_t raw;
            memcpy(&raw, &value, sizeof(raw));
            uint32_t exponent = (uint32_t)(raw >> 52) & 2047;
            uint64_t significand = raw & UINT64_C(0xfffffffffffff);
            if (exponent)
            {
                significand |= UINT64_C(1) << 52;
            }
            Decimal(significand, exponent ? (int)exponent - 1075 : -1074, (raw >> 63) != 0);
        }
        void Value(int index, uint32_t depth)
        {
            if (index < 0 && index > LUA_REGISTRYINDEX)
            {
                index += lua_gettop(m_Lua) + 1;
            }
            int type = lua_type(m_Lua, index);
            switch (type)
            {
                case LUA_TNIL:
                {
                    Append("null", 4);
                    return;
                }
                case LUA_TLIGHTUSERDATA:
                {
                    if (!lua_touserdata(m_Lua, index))
                    {
                        Append("null", 4);
                        return;
                    }
                    return;
                }
                case LUA_TBOOLEAN:
                {
                    if (lua_toboolean(m_Lua, index))
                    {
                        Append("true", 4);
                    }
                    else
                    {
                        Append("false", 5);
                    }
                    return;
                }
                case LUA_TSTRING:
                {
                    String(index);
                    return;
                }
                case LUA_TNUMBER:
                {
                    Number(index);
                    return;
                }
                case LUA_TUSERDATA:
                case LUA_TFUNCTION:
                case LUA_TTHREAD:
                {
                    luaL_checkstack(m_Lua, 3, "JSON metadata requires three stack slots");
                    lua_getglobal(m_Lua, "tostring");
                    lua_pushvalue(m_Lua, index);
                    lua_pcall(m_Lua, 1, 1, 0);
                    String(-1);
                    lua_pop(m_Lua, 1);
                    return;
                }
                case LUA_TTABLE:
                {
                    break;
                }
                default:
                {
                    luaL_error(m_Lua, "Cannot serialise %s: type not supported", lua_typename(m_Lua, type));
                    return;
                }
            }
            if (++depth > 1000 || !lua_checkstack(m_Lua, 3))
            {
                luaL_error(m_Lua, "Cannot serialise, excessive nesting (%d)", depth);
                return;
            }
            uint32_t count = 0, maximum = 0;
            bool array = true;
            lua_pushnil(m_Lua);
            while (lua_next(m_Lua, index))
            {
                double key = lua_tonumber(m_Lua, -2);
                if (lua_type(m_Lua, -2) != LUA_TNUMBER || key < 1 || key > INT_MAX || floor(key) != key)
                {
                    lua_pop(m_Lua, 2);
                    array = false;
                    break;
                }
                if (key > maximum)
                {
                    maximum = (uint32_t)key;
                }
                ++count;
                lua_pop(m_Lua, 1);
            }
            if (array && maximum > 10 && maximum / 2 + maximum % 2 > count)
            {
                luaL_error(m_Lua, "Cannot serialise table: excessively sparse array");
                return;
            }
            if (!count && m_EmptyObject)
            {
                array = false;
            }
            Byte(array ? '[' : '{');
            if (array)
            {
                for (uint32_t i = 1; i <= maximum; ++i)
                {
                    if (i > 1)
                    {
                        Byte(',');
                    }
                    lua_rawgeti(m_Lua, index, (int)i);
                    Value(-1, depth);
                    lua_pop(m_Lua, 1);
                }
            }
            else
            {
                count = 0;
                lua_pushnil(m_Lua);
                while (lua_next(m_Lua, index))
                {
                    if (count++)
                    {
                        Byte(',');
                    }
                    int key_type = lua_type(m_Lua, -2);
                    if (key_type == LUA_TSTRING)
                    {
                        String(-2);
                    }
                    else if (key_type == LUA_TNUMBER)
                    {
                        Byte('"');
                        Number(-2);
                        Byte('"');
                    }
                    else
                    {
                        luaL_error(m_Lua, "Cannot serialise %s: table key must be a number or string", lua_typename(m_Lua, key_type));
                        return;
                    }
                    Byte(':');
                    Value(-1, depth);
                    lua_pop(m_Lua, 1);
                }
            }
            Byte(array ? ']' : '}');
            return;
        }
    };

    static int EncodeProtected(lua_State* L)
    {
        JsonWriter* writer = (JsonWriter*)lua_touserdata(L, 1);
        writer->m_Lua = L;
        writer->Reserve(0);
        writer->Value(2, 0);
        return 0;
    }

    // Lua errors longjmp past C++ destructors. Own the output outside the
    // protected call so errors release it before propagating to the caller.
    int EncodeJson(lua_State* L, int index, int options, char** json, size_t* length)
    {
        int top = lua_gettop(L);
        if (index < 0 && index > LUA_REGISTRYINDEX)
        {
            index += top + 1;
        }
        bool empty_object = true;
        if (lua_istable(L, options))
        {
            lua_getfield(L, options, "encode_empty_table_as_object");
            if (!lua_isnil(L, -1))
            {
                empty_object = lua_toboolean(L, -1) != 0;
            }
            lua_pop(L, 1);
        }
        luaL_checkstack(L, 3, "JSON encoding requires three stack slots");
        int status;
        {
            JsonWriter writer(empty_object);
            lua_pushcfunction(L, EncodeProtected);
            lua_pushlightuserdata(L, &writer);
            lua_pushvalue(L, index);
            status = lua_pcall(L, 2, 0, 0);
            if (!status)
            {
                writer.m_Buffer.m_Data[writer.m_Size] = 0;
                *json = writer.m_Buffer.m_Data;
                *length = writer.m_Size;
                writer.m_Buffer.m_Data = 0;
            }
        }
        if (status)
        {
            return luaL_error(L, "%s", lua_tostring(L, -1));
        }
        return 1;
    }
}
