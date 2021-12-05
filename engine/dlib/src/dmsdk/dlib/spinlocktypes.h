#ifndef DMSDK_SPINLOCKTYPES_H
#define DMSDK_SPINLOCKTYPES_H

#if defined(_MSC_VER) || defined(__EMSCRIPTEN__)
#include "spinlocktypes_atomic.h"
#elif defined(ANDROID)
#include "spinlocktypes_android.h"
#elif defined(__linux__)
#include "spinlocktypes_pthread.h"
#elif defined(__MACH__)
#include "spinlocktypes_mach.h"
#elif defined(__NX64__)
#include "spinlocktypes_nx64.h"
#elif defined(__SCE__)
#include "spinlocktypes_ps4.h"
#else
#error "Unsupported platform"
#endif

#endif // DMSDK_SPINLOCKTYPES_H
