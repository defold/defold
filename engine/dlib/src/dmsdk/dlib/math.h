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

#ifndef DMSDK_MATH_H
#define DMSDK_MATH_H

#include <stdint.h>

/*# SDK Math API documentation
 * Math functions.
 *
 * @document
 * @name Math
 * @namespace dmMath
 * @language C++
 */

namespace dmMath
{

    /*# Min function
     * @name Min
     * @tparam T
     * @param a [type:T] Value a
     * @param b [type:T] Value b
     * @return v [type:T] Min of a and b
     */
    template <class T>
    const T Min(const T a, const T b)
    {
        return (a < b) ? a : b;
    }

    /*#
     * Max function
     * @name Max
     * @tparam T
     * @param a [type:T] Value a
     * @param b [type:T] Value b
     * @return v [type:T] Max of a and b
     */
    template <class T>
    const T Max(const T a, const T b)
    {
        return (a > b) ? a : b;
    }

    /*#
     * Clamp function
     * @name Clamp
     * @tparam T
     * @param v [type:T] Value to clamp
     * @param min [type:T] Lower bound
     * @param max [type:T] Upper bound
     * @return v [type:T] Value closest to v inside the range [min, max]
     */
    template <class T>
    const T Clamp(const T v, const T min, const T max)
    {
        return (v < min) ? min : (v > max) ? max : v;
    }

    /*# Select one of two values
     * Select one of two values depending on the sign of another.
     * @name Select
     * @tparam T
     * @param x [type:T] Value to test for positiveness
     * @param a [type:T] Result if test succeeded
     * @param b [type:T] Result if test failed
     * @return v [type:T] a when x >= 0, b otherwise
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
     * @tparam T
     * @param x [type:T]
     * @return v [type:T] Absolute value of x
     */
    template <class T>
    inline T Abs(T x)
    {
        return Select(x, x, -x);
    }

} // dmMath

#endif // DMSDK_MATH_H
