#ifndef DM_TIME_H
#define DM_TIME_H

#include <stdint.h>


namespace dmTime
{
    /**
     * Sleep thread
     * @param useconds Time to sleep in microseconds
     */
    void Sleep(uint32_t useconds);

    /**
     * Get current time in microseconds since since Jan. 1, 1970.
     * @return
     */
    uint64_t GetTime();
}

#endif // DM_TIME_H
