#include "time.h"

#if defined(__linux__) or defined(__MACH__)
#include <unistd.h>

#else
#error "Unsupported platform"
#endif

void dmSleep(uint32_t useconds)
{
#if defined(__linux__) or defined(__MACH__)
    usleep(useconds);
#else
#error "Unsupported platform"
#endif
}

