#ifndef DM_SHARED_LIBRARY_H
#define DM_SHARED_LIBRARY_H

/**
 * macro for shared library exports
 */
#ifdef _MSC_VER
#define DM_DLLEXPORT __declspec(dllexport)
#else
#define DM_DLLEXPORT
#endif

#endif // DM_SHARED_LIBRARY_H
