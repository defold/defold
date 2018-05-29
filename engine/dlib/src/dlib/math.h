#ifndef DM_MATH_H
#define DM_MATH_H

#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <stdint.h>
#if defined(_MSC_VER)
#undef _USE_MATH_DEFINES
#endif

/**
 * Collection of math functions.
 */
namespace dmMath
{
#define DM_RAND_MAX (0x7fff)
#define DM_RAND_MAX_RECIP (0.000030518509476f)

    /**
     * Min function
     * @param a Value a
     * @param b Value b
     * @return Min of a and b
     */
    template <class T>
    const T Min(const T a, const T b)
    {
        return (a < b) ? a : b;
    }

    /**
     * Max function
     * @param a Value a
     * @param b Value b
     * @return Max of a and b
     */
    template <class T>
    const T Max(const T a, const T b)
    {
        return (a > b) ? a : b;
    }

    /**
     * Clamp function
     * @param v Value to clamp
     * @param min Lower bound
     * @param max Upper bound
     * @return Value closest to v inside the range [min, max]
     */
    template <class T>
    const T Clamp(const T v, const T min, const T max)
    {
        return (v < min) ? min : (v > max) ? max : v;
    }

    /**
     * Linear Bezier curve (linear interpolation):
     * B(t) = p0 + t(p1 - p0) = (1 - t)p0 + t * p1, t in [0,1]
     * @param t Interpolator, should be inside [0,1]
     * @param p0 First sample, interpolation start
     * @param p1 Second sample, interpolation target
     * @return B(t)
     */
    template<class T>
    const T LinearBezier(const float t, const T p0, const T p1)
    {
        return p0 + t * (p1 - p0);
    }

    /**
     * Quadratic Bezier curve (parabolic):
     * B(t) = (1 - t)^2 * p0 + 2(1 - t)t * p1 + t^2 * p2, t in [0,1]
     * @param t Interpolator, should be inside [0,1]
     * @param p0 First sample, interpolation start
     * @param p1 Second sample, direction of parabola arc
     * @param p2 Third sample, interpolation target
     * @return B(t)
     */
    template<class T>
    const T QuadraticBezier(const float t, const T p0, const T p1, const T p2)
    {
        float s = (1.0f - t);
        return s * s * p0 + 2 * s * t * p1 + t * t * p2;
    }

    /**
     * Cubic Bezier curve (cubic Hermite):
     * B(t) = (1-t)^3 * p0 + 3(1 - t)^2 * t * p1 + 3(1 - t) * t^2 * p2 + t^3 * p3, t in [0,1]
     * @param t Interpolator, should be inside [0,1]
     * @param p0 First sample, interpolation start
     * @param p1 Second sample, p0 -> p1 forms the tangent at p0
     * @param p2 Third sample, p2 -> p3 forms the tangent at p3
     * @param p3 Fourth sample, interpolation target
     * @return B(t)
     */
    template<class T>
    const T CubicBezier(const float t, const T p0, const T p1, const T p2, const T p3)
    {
        float s = (1.0f - t);
        return s * s * s * p0 + 3.0f * s * s * t * p1 + 3.0f * s * t * t * p2 + t * t * t * p3;
    }

    /**
     * Select one of two floats depending on the sign of another.
     * @param x Value to test for positiveness
     * @param a Result if test succeeded
     * @param b Result if test failed
     * @return a when x >= 0, b otherwise
     */
    inline float Select(float x, float a, float b)
    {
        if (x >= 0.0f)
            return a;
        else
            return b;
    }

    /**
     * Abs function
     * @param x
     * @return Absolute value of x
     */
    inline float Abs(float x)
    {
        return Select(x, x, -x);
    }

    /**
     * Return a random number in the interval [0,DM_RAND_MAX].
     */
    inline uint32_t Rand(uint32_t* seed)
    {
        *seed = (214013 * *seed + 2531011);
        return (*seed >> 16) & DM_RAND_MAX;
    }

    /**
     * Return a random number in the interval [0,1].
     */
    inline float Rand01(uint32_t* seed)
    {
        return Rand(seed) * DM_RAND_MAX_RECIP;
    }

    /**
     * Return a random number in the interval [0,1).
     */
    inline float RandOpen01(uint32_t* seed)
    {
        return (Rand(seed)%DM_RAND_MAX) * DM_RAND_MAX_RECIP;
    }

    /**
     * Return a random number in the interval [-1,1].
     */
    inline float Rand11(uint32_t* seed)
    {
        return 2.0f * Rand01(seed) - 1.0f;
    }

}

#endif // DM_MATH_H
