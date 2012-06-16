#ifndef DM_ALIGN_H
#define DM_ALIGN_H

/**
 * Alignment macro. For compiler compatibility use the macro
 * between the type and the variable, e.g. int DM_ALIGNED(16) x;
 * @param a number of bytes to align to
 */
#define DM_ALIGNED(a)
#undef DM_ALIGNED

/**
 * Align a value to a boundary.
 * @param x value to align
 * @param a alignment boundary
 */
#define DM_ALIGN(x, a) (((uintptr_t) (x) + (a-1)) & ~(a-1))

#if defined(__GNUC__)
#define DM_ALIGNED(a) __attribute__ ((aligned (a)))
#elif defined(_MSC_VER)
#define DM_ALIGNED(a) __declspec(align(a))
#else
#error "Unsupported compiler"
#endif

#endif // DM_ALIGN_H
