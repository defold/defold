// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_MATH_H
#define DMSDK_MATH_H

#include <stdint.h>

/*# SDK Math API documentation
 * Math functions.
 *
 * @document
 * @name Math
 * @namespace dmMath
 * @path engine/dlib/src/dmsdk/dlib/math.h
 */

namespace dmMath
{

    /*# Min function
     * @name Min
     * @param [type:class T] a Value a
     * @param [type:class T] b Value b
     * @return v [type:class T] Min of a and b
     */
    template <class T>
    const T Min(const T a, const T b)
    {
        return (a < b) ? a : b;
    }

    /*#
     * Max function
     * @param [type:class T] a Value a
     * @param [type:class T] b Value b
     * @return v [type:class T] Max of a and b
     */
    template <class T>
    const T Max(const T a, const T b)
    {
        return (a > b) ? a : b;
    }

    /*#
     * Clamp function
     * @param [type:class T] v Value to clamp
     * @param [type:class T] min Lower bound
     * @param [type:class T] max Upper bound
     * @return v [type:class T] Value closest to v inside the range [min, max]
     */
    template <class T>
    const T Clamp(const T v, const T min, const T max)
    {
        return (v < min) ? min : (v > max) ? max : v;
    }

    /*# Select one of two values
     * Select one of two values depending on the sign of another.
     * @name Select
     * @param [type:class T] x Value to test for positiveness
     * @param [type:class T] a Result if test succeeded
     * @param [type:class T] b Result if test failed
     * @return v [type:class T] a when x >= 0, b otherwise
     */
    template <class T>
    inline T Select(T x, T a, T b)
    {
        if (x >= 0.0f)
            return a;
        else
            return b;
    }

    /*# Abs function
     * @name Abs
     * @param [type:class T] x
     * @return v [type:class T] Absolute value of x
     */
    template <class T>
    inline T Abs(T x)
    {
        return Select(x, x, -x);
    }

} // dmMath

#endif // DMSDK_MATH_H
