#include "time.h"

#if defined(__linux__) || defined(__MACH__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "Unsupported platform"
#endif

void dmSleep(uint32_t useconds)
{
#if defined(__linux__) || defined(__MACH__)
    usleep(useconds);
#elif defined(_WIN32)
    Sleep(useconds / 1000);
#else
#error "Unsupported platform"
#endif
}

