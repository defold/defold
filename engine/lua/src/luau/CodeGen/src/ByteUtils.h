// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"

#if defined(LUAU_BIG_ENDIAN)
#include <endian.h>
#endif

#include <string.h>

inline uint8_t* writeu8(uint8_t* target, uint8_t value)
{
    *target = value;
    return target + sizeof(value);
}

inline uint8_t* writeu16(uint8_t* target, uint16_t value)
{
#if defined(LUAU_BIG_ENDIAN)
    value = htole16(value);
#endif

    memcpy(target, &value, sizeof(value));
    return target + sizeof(value);
}

inline uint8_t* writeu32(uint8_t* target, uint32_t value)
{
#if defined(LUAU_BIG_ENDIAN)
    value = htole32(value);
#endif

    memcpy(target, &value, sizeof(value));
    return target + sizeof(value);
}

inline uint8_t* writeu64(uint8_t* target, uint64_t value)
{
#if defined(LUAU_BIG_ENDIAN)
    value = htole64(value);
#endif

    memcpy(target, &value, sizeof(value));
    return target + sizeof(value);
}

inline uint8_t* writeuleb128(uint8_t* target, uint64_t value)
{
    do
    {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (value)
            byte |= 0x80;

        *target++ = byte;
    } while (value);

    return target;
}

inline uint8_t* writef32(uint8_t* target, float value)
{
#if defined(LUAU_BIG_ENDIAN)
    static_assert(sizeof(float) == sizeof(uint32_t), "type size must match to reinterpret data");
    uint32_t data;
    memcpy(&data, &value, sizeof(value));
    writeu32(target, data);
#else
    memcpy(target, &value, sizeof(value));
#endif

    return target + sizeof(value);
}

inline uint8_t* writef64(uint8_t* target, double value)
{
#if defined(LUAU_BIG_ENDIAN)
    static_assert(sizeof(double) == sizeof(uint64_t), "type size must match to reinterpret data");
    uint64_t data;
    memcpy(&data, &value, sizeof(value));
    writeu64(target, data);
#else
    memcpy(target, &value, sizeof(value));
#endif

    return target + sizeof(value);
}
