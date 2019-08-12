#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#define DM_PLATFORM_LINUX   "linux"
#define DM_PLATFORM_OSX     "osx"
#define DM_PLATFORM_WINDOWS "windows"
#define DM_PLATFORM_WEB     "web"
#define DM_PLATFORM_ANDROID "android"
#define DM_PLATFORM_IOS 	"ios"

#if defined(ANDROID)
#define DM_PLATFORM DM_PLATFORM_ANDROID
#elif defined(__linux__)
#define DM_PLATFORM DM_PLATFORM_LINUX
#elif defined(__MACH__) && (defined(__arm__) || defined(__arm64__))
#define DM_PLATFORM DM_PLATFORM_IOS
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
