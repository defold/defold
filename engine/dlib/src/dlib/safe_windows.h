#if defined(_MSC_VER)

#ifndef SAFE_WINDOWS_H_
#define SAFE_WINDOWS_H_

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#ifdef PlaySound
#undef PlaySound
#endif

#ifdef DrawText
#undef DrawText
#endif

#ifdef DispatchMessage
#undef DispatchMessage
#endif

#ifdef FreeModule
#undef FreeModule
#endif

#ifdef GetTextMetrics
#undef GetTextMetrics
#endif

#undef MAX_TOUCH_COUNT

#endif // SAFE_WINDOWS_H_

#endif // defined(_MSC_VER)
