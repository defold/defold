#ifndef __SHARED_LIBRARY_H__
#define __SHARED_LIBRARY_H__

/**
 * macro for shared library exports
 */
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#endif // __SHARED_LIBRARY_H__
