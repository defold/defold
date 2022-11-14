#ifndef DMSDK_SOCKETTYPES_H
#define DMSDK_SOCKETTYPES_H

#if defined(__NX64__)
#include "sockettypes_nx64.h"
#elif defined(__SCE__)
#include "sockettypes_ps4.h"
#elif defined(_MSC_VER)
#include "sockettypes_win32.h"
#else
#include "sockettypes_posix.h"
#endif

#endif // DMSDK_SOCKETTYPES_H
