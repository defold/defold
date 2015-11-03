#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#define DM_PLATFORM_LINUX   "linux"
#define DM_PLATFORM_TVOS    "tvos"
#define DM_PLATFORM_OSX     "osx"
#define DM_PLATFORM_WINDOWS "windows"
#define DM_PLATFORM_WEB     "web"

#if defined(__linux__) // TODO: Android?
#define DM_PLATFORM DM_PLATFORM_LINUX
#elif defined(__TVOS__)
#define DM_PLATFORM DM_PLATFORM_TVOS
#elif defined(__MACH__)
#define DM_PLATFORM DM_PLATFORM_OSX
#elif defined(_WIN32)
#define DM_PLATFORM DM_PLATFORM_WINDOWS
#elif defined(__EMSCRIPTEN__)
#define DM_PLATFORM DM_PLATFORM_WEB
#elif defined(__AVM2__)
#define DM_PLATFORM DM_PLATFORM_WEB
#else
#error "Unsupported platform"
#endif

#endif
