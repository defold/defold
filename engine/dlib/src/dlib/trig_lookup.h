// This file was generated with the command:
// python scripts/gen_trig_lookup.py --bits=7

#ifndef DM_TRIG_LOOKUP_H
#define DM_TRIG_LOOKUP_H

#include "math.h"

/**
 * Collection of functions for trigonometrics using lookup tables.
 *
 * The precision is guaranteed to have an error less than 0.001f.
 */
namespace dmTrigLookup
{
    /// Contains the cosine of [0,2*pi) mapped into [0, COS_TABLE_SIZE-1]
    extern const float* COS_TABLE;
    /// Size of the table
    const uint32_t COS_TABLE_SIZE = 128;

    /**
     * Returns the cosine of the given angle from a lookup table.
     *
     * @param radians Radians of the angle
     * @return The cosine of the angle
     */
    inline float Cos(float radians)
    {
        // index is mapped to full 16 bit range
        uint16_t index = (uint16_t)(0xffff & (int32_t)(radians * (0x8000 * M_1_PI)));
        // t is normalized over the table cell range
        float t = (index & 511) * 0.001953125f;
        // remap index to actual range
        index = 127 & (index >> (16 - 7));
        // retrieve value and next for interpolation
        float v0 = COS_TABLE[index];
        float v1 = COS_TABLE[(index + 1) & 127];
        // linear interpolation
        return (1.0f - t) * v0 + t * v1;
    }

    /**
     * Returns the sine of the given angle from a lookup table.
     *
     * @param radians Radians of the angle
     * @return The sine of the angle
     */
    inline float Sin(float radians)
    {
        return Cos(radians - M_PI_2);
    }

}

#endif // DM_TRIG_LOOKUP_H
