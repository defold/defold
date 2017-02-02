#ifndef DMSDK_SHARED_LIBRARY_H
#define DMSDK_SHARED_LIBRARY_H

/**
 * macro for shared library exports
 */
#ifdef _MSC_VER
#define DM_DLLEXPORT __declspec(dllexport)
#else
#define DM_DLLEXPORT
#endif

#endif // DMSDK_SHARED_LIBRARY_H
