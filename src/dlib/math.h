#ifndef DM_MATH_H
#define DM_MATH_H

namespace dmMath
{
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
}

#endif
