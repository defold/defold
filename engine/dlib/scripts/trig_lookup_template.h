// This file was generated with the command:
// python scripts/gen_trig_lookup.py

#ifndef DM_TRIG_LOOKUP_H
#define DM_TRIG_LOOKUP_H

#include "math.h"

/**
 * Collection of functions for trigonometrics using lookup tables.
 */
namespace dmTrigLookup
{{
    const uint32_t TABLE_SIZE = {size};

    const float RATIO = 0.5f * M_1_PI;

    const float COS_TABLE[] = {table};

    /**
     * Returns the cosine of the given angle from a lookup table.
     *
     * @param radians Radians of the angle
     * @return The cosine of the angle
     */
    float Cos(float radians)
    {{
        float t = radians * RATIO;
        uint16_t index = (uint16_t)(0xffff & (uint32_t)(t * TABLE_SIZE));
        return COS_TABLE[index];
    }}

    /**
     * Returns the sine of the given angle from a lookup table.
     *
     * @param radians Radians of the angle
     * @return The sine of the angle
     */
    float Sin(float radians)
    {{
        return Cos(radians - M_PI_2);
    }}

}}

#endif // DM_TRIG_LOOKUP_H
