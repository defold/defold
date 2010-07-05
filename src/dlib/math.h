#ifndef DM_MATH_H
#define DM_MATH_H

/**
 * Min function
 * @param a Value a
 * @param b Value b
 * @return Min of a and b
 */
template <class T>
const T dmMin(const T a, const T b)
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
const T dmMax(const T a, const T b)
{
    return (a > b) ? a : b;
}

#endif
