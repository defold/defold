#ifndef DMSDK_SHARED_LIBRARY_H
#define DMSDK_SHARED_LIBRARY_H


/*# SDK Shared library API documentation
 * [file:<dmsdk/dlib/shared_library.h>]
 *
 * Utility functions for shared library export/import
 *
 * @document
 * @name Shared Library
 * @namespace sharedlibrary
*/

/*# storage-class attribute for shared library export/import
 *
 * Export and import functions, data, and objects to or from a DLL
 *
 * @macro
 * @name DM_DLLEXPORT
 * @examples
 *
 * ```cpp
 * DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);
 * ```
 *
 */
#ifdef _MSC_VER
#define DM_DLLEXPORT __declspec(dllexport)
#else
#define DM_DLLEXPORT __attribute__ ((visibility ("default")))
#endif

#endif // DMSDK_SHARED_LIBRARY_H
