// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"

#if defined(LUAU_ASSERTENABLED)
#define CODEGEN_ASSERT(expr) ((void)(!!(expr) || (Luau::assertCallHandler(#expr, __FILE__, __LINE__, __FUNCTION__) && (LUAU_DEBUGBREAK(), 0))))
#elif defined(CODEGEN_ENABLE_ASSERT_HANDLER)
#define CODEGEN_ASSERT(expr) ((void)(!!(expr) || Luau::assertCallHandler(#expr, __FILE__, __LINE__, __FUNCTION__)))
#else
#define CODEGEN_ASSERT(expr) (void)sizeof(!!(expr))
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define CODEGEN_TARGET_X64
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CODEGEN_TARGET_A64
#endif
