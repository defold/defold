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

#ifndef DMSDK_VMATH_H
#define DMSDK_VMATH_H

#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include "math.h"
#include <dmsdk/dlib/trig_lookup.h>

/*# Vector Math API documentation
 * Vector Math functions.
 *
 * @document
 * @name Vector Math
 * @namespace dmVMath
 * @language C++
 */
namespace dmVMath
{
    /*# 3-tuple
     * A 3-tuple (with 4-th element always set to 1)
     * @typedef
     * @name Point3
     * @note 16 byte aligned
     * @note Always size of 4 float32
     * @note Currently scalar implementation is used on most platforms
     * @examples
     * ```cpp
     * dmVMath::Point3 p = dmVMath::Point3(x, y, z); // Create new point
     * float length_squared = p.getX() * p.getX() + p.getY() * p.getY() + p.getZ() * p.getZ();
     * ```
     */
    typedef Vectormath::Aos::Point3     Point3;

    /*# 3-tuple
     * A 3-tuple (with 4-th element always set to 0)
     * @typedef
     * @name Vector3
     * @note 16 byte aligned
     * @note Always size of 4 float32
     * @note Currently scalar implementation is used on most platforms
     * @examples
     * ```cpp
     * dmVMath::Vector3 p = dmVMath::Vector3(x, y, z); // Create new vector
     * float length_squared = p.getX() * p.getX() + p.getY() * p.getY() + p.getZ() * p.getZ();
     * ```
     */
    typedef Vectormath::Aos::Vector3    Vector3;

     /*# 4-tuple
     * A 4-tuple
     * @typedef
     * @name Vector4
     * @note 16 byte aligned
     * @note Always size of 4 float32
     * @note Currently scalar implementation is used on most platforms
     * @examples
     * ```cpp
     * dmVMath::Vector4 p = dmVMath::Vector4(x, y, z, w); // Create new vector
     * float length_squared = p.getX() * p.getX() + p.getY() * p.getY() + p.getZ() * p.getZ() + p.getW() * p.getW();
     * ```
     */
    typedef Vectormath::Aos::Vector4    Vector4;

    /*# 4-tuple representing a rotation
     * A 4-tuple representing a rotation rotation. The `xyz` represents the axis, and the `w` represents the angle.
     * @typedef
     * @name Quat
     * @note 16 byte aligned
     * @note Always size of 4 float32
     * @note Currently scalar implementation is used on most platforms
     * @examples
     * ```cpp
     * dmVMath::Quat p = dmVMath::Quat(x, y, z, w); // Create new rotation. W is the angle
     * ```
     */
    typedef Vectormath::Aos::Quat       Quat;

    /*# 3x3 matrix
     * A 3x3 matrix
     * @typedef
     * @name Matrix3
     * @note 16 byte aligned
     * @note Implemented as 3 x Vector3
     * @note Column major
     * @note Currently scalar implementation is used on most platforms
     */
    typedef Vectormath::Aos::Matrix3    Matrix3;

    /*# 4x4 matrix
     * A 4x4 matrix
     * @typedef
     * @name Matrix4
     * @note 16 byte aligned
     * @note Implemented as 4 x Vector4
     * @note Column major
     * @note Currently scalar implementation is used on most platforms
     */
    typedef Vectormath::Aos::Matrix4    Matrix4;

    /*# construct a quaternion from an axis index and angle
     * @name QuatFromAngle
     * @param axis_index [type: uint32_t] index of the rotation axis
     * @param radians [type: float] angle of rotation in radians
     * @return q [type: Quat] quaternion describing the rotation
     */
    inline dmVMath::Quat QuatFromAngle(uint32_t axis_index, float radians)
    {
        dmVMath::Quat q(0, 0, 0, 1);
        float half_angle = 0.5f * radians;
        q.setElem(axis_index, dmTrigLookup::Sin(half_angle));
        q.setW(dmTrigLookup::Cos(half_angle));
        return q;
    }

#define DEG_FACTOR 57.295779513f

    /*#
     * Converts a quaternion into euler angles (r0, r1, r2), based on YZX rotation order.
     * To handle gimbal lock (singularity at r1 ~ +/- 90 degrees), the cut off is at r0 = +/- 88.85 degrees, which snaps to +/- 90.
     * The provided quaternion is expected to be normalized.
     * The error is guaranteed to be less than +/- 0.02 degrees
     * @name QuatToEuler
     * @param q0 [type: float] first imaginary axis
     * @param q1 [type: float] second imaginary axis
     * @param q2 [type: float] third imaginary axis
     * @param q3 [type: float] real part
     * @return euler [type: Vector3] euler angles in degrees
     */
    inline dmVMath::Vector3 QuatToEuler(float q0, float q1, float q2, float q3)
    {
        // Early-out when the rotation axis is either X, Y or Z.
        // The reasons we make this distinction is that one-axis rotation is common (and cheaper), especially around Z in 2D games
        uint8_t mask = (q2 != 0.f) << 2 | (q1 != 0.f) << 1 | (q0 != 0.f);
        switch (mask) {
        case 0b000:
            return dmVMath::Vector3(0.0f, 0.0f, 0.0f);
        case 0b001:
        case 0b010:
        case 0b100:
            {
                dmVMath::Vector3 r(0.0f, 0.0f, 0.0f);
                // the sum of the values yields one value, as the others are 0
                r.setElem(mask >> 1, atan2f(q0+q1+q2, q3) * 2.0f * DEG_FACTOR);
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
            r1 = 2.0f * atan2f(q0, q3);
            r2 = (float) M_PI_2;
            r0 = 0.0f;
        }
        else if (test < -limit)
        {
            r1 = -2.0f * atan2f(q0, q3);
            r2 = (float) -M_PI_2;
            r0 = 0.0f;
        }
        else
        {
            float sq0 = q0 * q0;
            float sq1 = q1 * q1;
            float sq2 = q2 * q2;
            r1 = atan2f(2.0f * q1 * q3 - 2.0f * q0 * q2, 1.0f - 2.0f * sq1 - 2.0f * sq2);
            r2 = asinf(2.0f * test);
            r0 = atan2f(2.0f * q0 * q3 - 2.0f * q1 * q2, 1.0f - 2.0f * sq0 - 2.0f * sq2);
        }
        return dmVMath::Vector3(r0, r1, r2) * DEG_FACTOR;
    }

#undef DEG_FACTOR
#define HALF_RAD_FACTOR 0.008726646f

    /*#
     * Converts euler angles (x, y, z) in degrees into a quaternion
     * The error is guaranteed to be less than 0.001.
     * @name EulerToQuat
     * @param xyz [type: Vector3]  rotation (deg)
     * @result rot [type: Quat] Quat describing an equivalent rotation (231 (YZX) rotation sequence).
     */
    inline dmVMath::Quat EulerToQuat(dmVMath::Vector3 xyz)
    {
        // Early-out when the rotation axis is either X, Y or Z.
        // The reasons we make this distinction is that one-axis rotation is common (and cheaper), especially around Z in 2D games
        uint8_t mask = (xyz.getZ() != 0.f) << 2 | (xyz.getY() != 0.f) << 1 | (xyz.getX() != 0.f);
        switch (mask) {
        case 0b000:
            return dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f);
        case 0b001:
        case 0b010:
        case 0b100:
            {
                // the sum of the angles yields one angle, as the others are 0
                float ha = (xyz.getX()+xyz.getY()+xyz.getZ()) * HALF_RAD_FACTOR;
                dmVMath::Quat q(0.0f, 0.0f, 0.0f, dmTrigLookup::Cos(ha));
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

        dmVMath::Quat quat;
        quat.setW(-s1*s2_s3 + c1_c2*c3 );
        quat.setX( s1*s2*c3 + s3*c1_c2 );
        quat.setY( s1*c2*c3 + s2_s3*c1 );
        quat.setZ(-s1*s3*c2 + s2*c1*c3 );
        return quat;
    }
#undef HALF_RAD_FACTOR

    /*# dot product between two vectors
     * @name Dot
     * @param a [type: Vector3] the first vector
     * @param b [type: Vector3] the second vector
     * @return dot_product [type: float] the dot product
     */
    inline float Dot(Vector3 a, Vector3 b)      { return Vectormath::Aos::dot(a, b); }

    /*# dot product between two vectors
     * @name Dot
     * @param a [type: Vector4] the first vector
     * @param b [type: Vector4] the second vector
     * @return dot_product [type: float] the dot product
     */
    inline float Dot(Vector4 a, Vector4 b)      { return Vectormath::Aos::dot(a, b); }

    /*# calculate length of a vector
     * @name Length
     * @param v [type: Vector3] the vector
     * @return length [type: float] the length
     */
    inline float Length(Vector3 v)              { return Vectormath::Aos::length(v); }

    /*# calculate length of a vector
     * @name Length
     * @param v [type: Vector3] the vector
     * @return length [type: float] the length
     */
    inline float Length(Vector4 v)              { return Vectormath::Aos::length(v); }

    /*# calculate length of a quaternion
     * @name Length
     * @param v [type: Quat] the quaternion
     * @return length [type: float] the length
     */
    inline float Length(Quat v)                 { return Vectormath::Aos::length(v); }

    /*# calculate squared length of a vector
     * @name Length
     * @param v [type: Vector3] the vector
     * @return length [type: float] the squared length
     */
    inline float LengthSqr(Vector3 v)           { return Vectormath::Aos::lengthSqr(v); }

    /*# calculate squared length of a vector
     * @name Length
     * @param v [type: Vector4] the vector
     * @return length [type: float] the squared length
     */
    inline float LengthSqr(Vector4 v)           { return Vectormath::Aos::lengthSqr(v); }

    /*# calculate squared length of a quaternion
     * @name Length
     * @param v [type: Quat] the vector
     * @return length [type: float] the squared length
     */
    inline float LengthSqr(Quat v)              { return Vectormath::Aos::norm(v); } // quat doesn't have a lengthSqr(), but this is what's called before the sqrtf in length()

    /*# normalize a vector to length 1
     * @name Normalize
     * @param v [type: Vector3] the vector
     * @return n [type: Vector3] the normalized vector
     */
    inline Vector3  Normalize(Vector3 v)        { return Vectormath::Aos::normalize(v); }

    /*# normalize a vector to length 1
     * @name Normalize
     * @param v [type: Vector4] the vector
     * @return n [type: Vector4] the normalized vector
     */
    inline Vector4  Normalize(Vector4 v)        { return Vectormath::Aos::normalize(v); }

    /*# normalize a quaternion to length 1
     * @name Normalize
     * @param v [type: Quat] the quaternion
     * @return n [type: Quat] the normalized quaternion
     */
    inline Quat     Normalize(Quat v)           { return Vectormath::Aos::normalize(v); }

    /*# linear interpolate between two vectors
     * @name Lerp
     * @param t [type: float] the unit time
     * @param a [type: Vector3] the start vector (t == 0)
     * @param b [type: Vector3] the end vector (t == 1)
     * @return v [type: Vector3] the result vector `v = a + (b - a) * t`
     * @note Does not clamp t to between 0 and 1
     * @examples
     * ```cpp
     * dmVMath::Vector3 v0 = dmVMath::Lerp(0.0f, a, b); // v0 == a
     * dmVMath::Vector3 v1 = dmVMath::Lerp(1.0f, a, b); // v1 == b
     * dmVMath::Vector3 v2 = dmVMath::Lerp(2.0f, a, b); // v2 == a + (b-a) * 2.0f
     * ```
     */
    inline Vector3  Lerp(float t, Vector3 a, Vector3 b)     { return Vectormath::Aos::lerp(t, a, b); }

    /*# linear interpolate between two vectors
     * @name Lerp
     * @param t [type: float] the unit time
     * @param a [type: Vector4] the start vector (t == 0)
     * @param b [type: Vector4] the end vector (t == 1)
     * @return v [type: Vector4] the result vector `v = a + (b - a) * t`
     * @note Does not clamp t to between 0 and 1
     * @examples
     * ```cpp
     * dmVMath::Vector4 v0 = dmVMath::Lerp(0.0f, a, b); // v0 == a
     * dmVMath::Vector4 v1 = dmVMath::Lerp(1.0f, a, b); // v1 == b
     * dmVMath::Vector4 v2 = dmVMath::Lerp(2.0f, a, b); // v2 == a + (b-a) * 2.0f
     * ```
     */
    inline Vector4  Lerp(float t, Vector4 a, Vector4 b)     { return Vectormath::Aos::lerp(t, a, b); }

    // Intentionally left undocumented for now since it's not as commonly used as Slerp
    inline Quat     Lerp(float t, Quat a, Quat b)           { return Vectormath::Aos::lerp(t, a, b); }

    /*# spherical linear interpolate between two vectors
     * @name Slerp
     * @param t [type: float] the unit time
     * @param a [type: Vector3] the start vector (t == 0)
     * @param b [type: Vector3] the end vector (t == 1)
     * @return v [type: Vector3] the result vector
     * @note Does not clamp t to between 0 and 1
     * @note Unpredicatable results if a and b point in opposite direction
     */
    inline Vector3  Slerp(float t, Vector3 a, Vector3 b)    { return Vectormath::Aos::slerp(t, a, b); }

    /*# spherical linear interpolate between two vectors
     * @name Slerp
     * @param t [type: float] the unit time
     * @param a [type: Vector4] the start vector (t == 0)
     * @param b [type: Vector4] the end vector (t == 1)
     * @return v [type: Vector4] the result vector
     * @note Does not clamp t to between 0 and 1
     * @note Unpredicatable results if a and b point in opposite direction
     */
    inline Vector4  Slerp(float t, Vector4 a, Vector4 b)    { return Vectormath::Aos::slerp(t, a, b); }

    /*# spherical linear interpolate between two vectors
     * Interpolates along the shortest path between two quaternions
     * @name Slerp
     * @param t [type: float] the unit time
     * @param a [type: Quat] the start vector (t == 0)
     * @param b [type: Quat] the end vector (t == 1)
     * @return v [type: Quat] the result vector
     * @note Does not clamp t to between 0 and 1
     */
    inline Quat     Slerp(float t, Quat a, Quat b)          { return Vectormath::Aos::slerp(t, a, b); }

    /*# divide two vectors per element
     * Divide two vectors per element: `Vector3(a.x/b.x, a.y/b.y, a.z/b.z)`
     * @name DivPerElem
     * @param a [type: Vector3] the operand
     * @param b [type: Vector3] the dividend
     * @return v [type: Vector3] the result vector
     */
    inline Vector3  DivPerElem(Vector3 a, Vector3 b)        { return Vectormath::Aos::divPerElem(a, b); }

    /*# divide two vectors per element
     * Divide two vectors per element: `Vector3(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w)`
     * @name DivPerElem
     * @param a [type: Vector4] the operand
     * @param b [type: Vector4] the dividend
     * @return v [type: Vector4] the result vector
     */
    inline Vector4  DivPerElem(Vector4 a, Vector4 b)        { return Vectormath::Aos::divPerElem(a, b); }

    /*# cross product between two vectors
     * @name Cross
     * @param a [type: Vector3] the operand
     * @param b [type: Vector3] the dividend
     * @return v [type: Vector3] the result vector
     */
    inline Vector3  Cross(Vector3 a, Vector3 b)             { return Vectormath::Aos::cross(a, b); }

    /*# multiply two vectors per element
     * Multiply two vectors per element: `Vector3(a.x * b.x, a.y * b.y, a.z * b.z)`
     * @name MulPerElem
     * @param a [type: Vector3] the first vector
     * @param b [type: Vector3] the second vector
     * @return v [type: Vector3] the result vector
     */
    inline Vector3  MulPerElem(Vector3 a, Vector3 b)        { return Vectormath::Aos::mulPerElem(a, b); }

    /*# multiply two vectors per element
     * Multiply two vectors per element: `Vector3(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w)`
     * @name MulPerElem
     * @param a [type: Vector4] the first vector
     * @param b [type: Vector4] the second vector
     * @return v [type: Vector4] the result vector
     */
    inline Vector4  MulPerElem(Vector4 a, Vector4 b)        { return Vectormath::Aos::mulPerElem(a, b); }

    /*# abs value per element
     * Return absolute value per element: `Vector3(abs(v.x), abs(v.y), abs(v.z))`
     * @name MulPerElem
     * @param v [type: Vector3] the vector
     * @return r [type: Vector3] the result vector
     */
    inline Vector3  AbsPerElem(Vector3 v)                   { return Vectormath::Aos::absPerElem(v); }

    /*# conjugate of quaternion
     * Returns the conjugate of the quaternion: `conj = -q`
     * @name MulPerElem
     * @param q [type: Quat] the quaternions
     * @return r [type: Quat] the result
     */
    inline Quat     Conjugate(Quat v)                       { return Vectormath::Aos::conj(v); }

    /*# rotate vector using quaternion
     * @name Rotate
     * @param q [type: Quat] the rotation
     * @param v [type: Vector3] the vector
     * @return r [type: Vector3] the rotated vector
     */
    inline Vector3  Rotate(Quat q, Vector3 v)               { return Vectormath::Aos::rotate(q, v); }

    /*# transpose matrix
     * @name Transpose
     * @param m [type: Matrix3] the rotation
     * @return r [type: Matrix3] the transposed matrix
     */
    inline Matrix3 Transpose(Matrix3 m)                     { return Vectormath::Aos::transpose(m); }

    /*# transpose matrix
     * @name Transpose
     * @param m [type: Matrix4] the rotation
     * @return r [type: Matrix4] the transposed matrix
     */
    inline Matrix4 Transpose(Matrix4 m)                     { return Vectormath::Aos::transpose(m); }

    /*# inverse matrix
     * @name Inverse
     * @param m [type: Matrix3] the rotation
     * @return r [type: Matrix3] the transposed matrix
     */
    inline Matrix3 Inverse(Matrix3 m)                       { return Vectormath::Aos::inverse(m); }

    /*# inverse matrix
     * @name Inverse
     * @param m [type: Matrix4] the rotation
     * @return r [type: Matrix4] the transposed matrix
     */
    inline Matrix4 Inverse(Matrix4 m)                       { return Vectormath::Aos::inverse(m); }

    /*#
     * Compute the inverse of a 4x4 matrix, which is expected to be an affine matrix with an orthogonal upper-left 3x3 submatrix
     * @name OrthoInverse
     * @param m [type: Matrix4] the rotation
     * @return r [type: Matrix4] the transposed matrix
     */
    inline Matrix4 OrthoInverse(Matrix4 m)                  { return Vectormath::Aos::orthoInverse(m); }

    /*# post multiply scale
     * @name AppendScale
     * @param m [type: Matrix4] the matrix
     * @param v [type: Vector3] the scale vector
     * @return r [type: Matrix4] the scaled vector
     */
    inline Matrix4 AppendScale(Matrix4 m, Vector3 v)        { return Vectormath::Aos::appendScale(m, v); }

} // dmVMath

#endif // DMSDK_VMATH_H
