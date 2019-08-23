#ifndef DMSDK_TIME_H
#define DMSDK_TIME_H

#include <stdint.h>

/*# SDK Time API documentation
 * [file:<dmsdk/dlib/time.h>]
 *
 * Time functions.
 *
 * @document
 * @name Time
 * @namespace dmTime
 */

namespace dmTime
{
    /*# get current time in microseconds
     *
     * Get current time in microseconds since Jan. 1, 1970.
     * @name dmTime::GetTime
     * @return result [type:uint64_t] Current time in microseconds
     */
    uint64_t GetTime();
}

#endif // DMSDK_TIME_H
