#ifndef DMSDK_ALIGN_H
#define DMSDK_ALIGN_H

/*# SDK Align API documentation
 * [file:<dmsdk/dlib/align.h>]
 *
 * Alignment macros. Use for compiler compatibility
 *
 * @document
 * @name Align
 * @namespace dmAlign
 */

/*# data structure alignment macro
 *
 * Data structure alignment
 *
 * @macro
 * @name DM_ALIGNED
 * @param a [type:int] number of bytes to align to
 *
 * @examples
 *
 * Align m_Data to 16 bytes alignment boundary:
 *
 * ```cpp
 * int DM_ALIGNED(16) m_Data;
 * ```
 */
#define DM_ALIGNED(a)
#undef DM_ALIGNED


/*# value alignment macro
 *
 * Align a value to a boundary
 *
 * @macro
 * @name DM_ALIGN
 * @param x [type:int] value to align
 * @param a [type:int] alignment boundary
 *
 * @examples
 *
 * Align 24 to 16 alignment boundary. results is 16:
 *
 * ```cpp
 * int result = DM_ALIGN(24, 16);
 * ```
 */
#define DM_ALIGN(x, a) (((uintptr_t) (x) + (a-1)) & ~(a-1))

#if defined(__GNUC__)
#define DM_ALIGNED(a) __attribute__ ((aligned (a)))
#elif defined(_MSC_VER)
#define DM_ALIGNED(a) __declspec(align(a))
#else
#error "Unsupported compiler"
#endif

#endif // DMSDK_ALIGN_H
