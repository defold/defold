#ifndef DM_VMATH_H
#define DM_VMATH_H

#include <vectormath/cpp/vectormath_aos.h>
#include "math.h"
#include "trig_lookup.h"

#include <assert.h>

/**
 * Collection of vector math functions.
 */
namespace dmVMath
{

    struct FloatVector
    {
        int size;
        float* values;

        FloatVector() { size = 0; values = NULL; };
        FloatVector(int new_size) {
            assert(new_size >= 0);
            size = new_size;
            if (new_size > 0)
                values = (float*)malloc(new_size * sizeof(float));
            else
                values = NULL;
        };

        ~FloatVector() {
            if (size > 0 && values)
            {
                free(values);
                values = NULL;
            }
        }
    };

    /**
     * Construct a quaternion from an axis index and angle
     * @param axis_index Index of the axis the quaternion should be a rotation around
     * @param radians Angle of rotation
     * @return Quaternion describing the rotation
     */
    inline Vectormath::Aos::Quat QuatFromAngle(uint32_t axis_index, float radians)
    {
        Vectormath::Aos::Quat q(0, 0, 0, 1);
        float half_angle = 0.5f * radians;
        q.setElem(axis_index, dmTrigLookup::Sin(half_angle));
        q.setW(dmTrigLookup::Cos(half_angle));
        return q;
    }

#define DEG_FACTOR 57.295779513f

    /**
     * Converts a quaternion into euler angles (r0, r1, r2), based on YZX rotation order.
     * To handle gimbal lock (singularity at r1 ~ +/- 90 degrees), the cut off is at r0 = +/- 88.85 degrees, which snaps to +/- 90.
     * The provided quaternion is expected to be normalized.
     * The error is guaranteed to be less than +/- 0.02 degrees
     * @param q0 first imaginary axis
     * @param q1 second imaginary axis
     * @param q2 third imaginary axis
     * @param q3 real part
     * @result Euler angles in degrees and the same order as the specified rotation order
     */
    inline Vectormath::Aos::Vector3 QuatToEuler(float q0, float q1, float q2, float q3)
    {
        // Early-out when the rotation axis is either X, Y or Z.
        // The reasons we make this distinction is that one-axis rotation is common (and cheaper), especially around Z in 2D games
        uint8_t mask = (q2 != 0.f) << 2 | (q1 != 0.f) << 1 | (q0 != 0.f);
        switch (mask) {
        case 0b001:
        case 0b010:
        case 0b100:
            {
                Vectormath::Aos::Vector3 r(0.0f, 0.0f, 0.0f);
                // the sum of the values yields one value, as the others are 0
                r.setElem(mask >> 1, atan2(q0+q1+q2, q3) * 2.0f * DEG_FACTOR);
                return r;
            }
        }
        // Implementation based on:
        // * http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
        // * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/
        // * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/Quaternions.pdf
        const float limit = 0.4999f; // gimbal lock limit, corresponds to 88.85 degrees
        float r0, r1, r2;
        float test = q0 * q1 + q2 * q3;
        if (test > limit)
        {
            r1 = 2.0f * atan2(q0, q3);
            r2 = (float) M_PI_2;
            r0 = 0.0f;
        }
        else if (test < -limit)
        {
            r1 = -2.0f * atan2(q0, q3);
            r2 = (float) -M_PI_2;
            r0 = 0.0f;
        }
        else
        {
            float sq0 = q0 * q0;
            float sq1 = q1 * q1;
            float sq2 = q2 * q2;
            r1 = atan2(2.0f * q1 * q3 - 2.0f * q0 * q2, 1.0f - 2.0f * sq1 - 2.0f * sq2);
            r2 = asin(2.0f * test);
            r0 = atan2(2.0f * q0 * q3 - 2.0f * q1 * q2, 1.0f - 2.0f * sq0 - 2.0f * sq2);
        }
        return Vectormath::Aos::Vector3(r0, r1, r2) * DEG_FACTOR;
    }

#undef DEG_FACTOR
#define HALF_RAD_FACTOR 0.008726646f

    /**
     * Converts euler angles (x, y, z) in degrees into a quaternion
     * The error is guaranteed to be less than 0.001.
     * @param x rotation around x-axis (deg)
     * @param y rotation around y-axis (deg)
     * @param z rotation around z-axis (deg)
     * @result Quat describing an equivalent rotation (231 (YZX) rotation sequence).
     */
    inline Vectormath::Aos::Quat EulerToQuat(Vectormath::Aos::Vector3 xyz)
    {
        // Early-out when the rotation axis is either X, Y or Z.
        // The reasons we make this distinction is that one-axis rotation is common (and cheaper), especially around Z in 2D games
        uint8_t mask = (xyz.getZ() != 0.f) << 2 | (xyz.getY() != 0.f) << 1 | (xyz.getX() != 0.f);
        switch (mask) {
        case 0b001:
        case 0b010:
        case 0b100:
            {
                // the sum of the angles yields one angle, as the others are 0
                float ha = (xyz.getX()+xyz.getY()+xyz.getZ()) * HALF_RAD_FACTOR;
                Vectormath::Aos::Quat q(0.0f, 0.0f, 0.0f, dmTrigLookup::Cos(ha));
                q.setElem(mask >> 1, dmTrigLookup::Sin(ha));
                return q;
            }
        }
        // Implementation based on:
        // http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
        // Rotation sequence: 231 (YZX)
        float t1 = xyz.getY() * HALF_RAD_FACTOR;
        float t2 = xyz.getZ() * HALF_RAD_FACTOR;
        float t3 = xyz.getX() * HALF_RAD_FACTOR;

        float c1 = dmTrigLookup::Cos(t1);
        float s1 = dmTrigLookup::Sin(t1);
        float c2 = dmTrigLookup::Cos(t2);
        float s2 = dmTrigLookup::Sin(t2);
        float c3 = dmTrigLookup::Cos(t3);
        float s3 = dmTrigLookup::Sin(t3);
        float c1_c2 = c1*c2;
        float s2_s3 = s2*s3;

        Vectormath::Aos::Quat quat;
        quat.setW(-s1*s2_s3 + c1_c2*c3 );
        quat.setX( s1*s2*c3 + s3*c1_c2 );
        quat.setY( s1*c2*c3 + s2_s3*c1 );
        quat.setZ(-s1*s3*c2 + s2*c1*c3 );
        return quat;
    }

#undef HALF_RAD_FACTOR

}

#endif
