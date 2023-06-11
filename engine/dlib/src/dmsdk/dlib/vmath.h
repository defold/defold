// Copyright 2020-2023 The Defold Foundation
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
    typedef Vectormath::Aos::Point3     Point3;
    typedef Vectormath::Aos::Vector3    Vector3;
    typedef Vectormath::Aos::Vector4    Vector4;
    typedef Vectormath::Aos::Quat       Quat;
    typedef Vectormath::Aos::Matrix4    Matrix4;
    typedef Vectormath::Aos::Matrix3    Matrix3;

    inline float Dot(Vector3 a, Vector3 b)      { return Vectormath::Aos::dot(a, b); }
    inline float Dot(Vector4 a, Vector4 b)      { return Vectormath::Aos::dot(a, b); }

    inline float Length(Vector3 v)              { return Vectormath::Aos::length(v); }
    inline float Length(Vector4 v)              { return Vectormath::Aos::length(v); }
    inline float Length(Quat v)                 { return Vectormath::Aos::length(v); }

    inline float LengthSqr(Vector3 v)           { return Vectormath::Aos::lengthSqr(v); }
    inline float LengthSqr(Vector4 v)           { return Vectormath::Aos::lengthSqr(v); }
    inline float LengthSqr(Quat v)              { return Vectormath::Aos::norm(v); } // quat doesn't have a lengthSqr(), but this is what's called before the sqrtf in length()

    inline Vector3  Normalize(Vector3 v)        { return Vectormath::Aos::normalize(v); }
    inline Vector4  Normalize(Vector4 v)        { return Vectormath::Aos::normalize(v); }
    inline Quat     Normalize(Quat v)           { return Vectormath::Aos::normalize(v); }

    inline Vector3  Lerp(float t, Vector3 a, Vector3 b)     { return Vectormath::Aos::lerp(t, a, b); }
    inline Vector4  Lerp(float t, Vector4 a, Vector4 b)     { return Vectormath::Aos::lerp(t, a, b); }
    inline Quat     Lerp(float t, Quat a, Quat b)           { return Vectormath::Aos::lerp(t, a, b); }

    inline Vector3  Slerp(float t, Vector3 a, Vector3 b)    { return Vectormath::Aos::slerp(t, a, b); }
    inline Vector4  Slerp(float t, Vector4 a, Vector4 b)    { return Vectormath::Aos::slerp(t, a, b); }
    inline Quat     Slerp(float t, Quat a, Quat b)          { return Vectormath::Aos::slerp(t, a, b); }

    inline Vector3  DivPerElem(Vector3 a, Vector3 b)        { return Vectormath::Aos::divPerElem(a, b); }
    inline Vector3  Cross(Vector3 a, Vector3 b)             { return Vectormath::Aos::cross(a, b); }
    inline Vector3  MulPerElem(Vector3 a, Vector3 b)        { return Vectormath::Aos::mulPerElem(a, b); }
    inline Vector4  MulPerElem(Vector4 a, Vector4 b)        { return Vectormath::Aos::mulPerElem(a, b); }
    inline Vector3  AbsPerElem(Vector3 v)                   { return Vectormath::Aos::absPerElem(v); }

    inline Quat     Conjugate(Quat v)                       { return Vectormath::Aos::conj(v); }
    inline Vector3  Rotate(Quat q, Vector3 v)               { return Vectormath::Aos::rotate(q, v); }

    inline Matrix3 Transpose(Matrix3 m)                     { return Vectormath::Aos::transpose(m); }
    inline Matrix4 Transpose(Matrix4 m)                     { return Vectormath::Aos::transpose(m); }
    inline Matrix3 Inverse(Matrix3 m)                       { return Vectormath::Aos::inverse(m); }
    inline Matrix4 Inverse(Matrix4 m)                       { return Vectormath::Aos::inverse(m); }

    inline Matrix4 OrthoInverse(Matrix4 m)                  { return Vectormath::Aos::orthoInverse(m); }

    inline Matrix4 AppendScale(Matrix4 m, Vector3 v)        { return Vectormath::Aos::appendScale(m, v); }

} // dmVMath

#endif // DMSDK_VMATH_H
