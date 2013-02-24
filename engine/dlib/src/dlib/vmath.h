#ifndef DM_VMATH_H
#define DM_VMATH_H

#include <vectormath/cpp/vectormath_aos.h>

#include "math.h"
#include "trig_lookup.h"

/**
 * Collection of vector math functions.
 */
namespace dmVMath
{

    /**
     * Construct a quaternion from an axis index and angle
     * @param axis_index Index of the axis the quaternion should be a rotation around
     * @param radians Angle of rotation
     * @return Quaternion describing the rotation
     */
    Vectormath::Aos::Quat QuatFromAngle(uint32_t axis_index, float radians)
    {
        Vectormath::Aos::Quat q(0, 0, 0, 1);
        float half_angle = 0.5f * radians;
        q.setElem(axis_index, dmTrigLookup::Sin(half_angle));
        q.setW(dmTrigLookup::Cos(half_angle));
        return q;
    }

}

#endif
