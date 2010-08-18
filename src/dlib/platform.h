#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#define DM_PLATFORM_LINUX   "linux"
#define DM_PLATFORM_OSX     "osx"
#define DM_PLATFORM_WINDOWS "windows"

#if defined(__linux__)
#define DM_PLATFORM DM_PLATFORM_LINUX
#elif defined(__MACH__)
#define DM_PLATFORM DM_PLATFORM_OSX
#elif defined(_WIN32)
#define DM_PLATFORM DM_PLATFORM_WINDOWS
#else
#error "Unsupported platform"
#endif

#endif
