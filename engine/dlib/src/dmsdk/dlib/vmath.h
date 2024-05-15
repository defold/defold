// Copyright 2020-2024 The Defold Foundation
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

/*# SDK Vector Math API documentation
 * Vector Math functions.
 *
 * @document
 * @name Vector Math
 * @namespace dmVMath
 * @path engine/dlib/src/dmsdk/dlib/vmath.h
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
