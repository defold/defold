// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_TIME_H
#define DMSDK_TIME_H

#include <stdint.h>

/*# SDK Time API documentation
 *
 * Time functions.
 *
 * @document
 * @name Time
 * @namespace dmTime
 * @path engine/dlib/src/dmsdk/dlib/time.h
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

    /*# sleep thread with low precision (~10 milliseconds).
     *
     * Sleep thread with low precision (~10 milliseconds).
     * @name dmTime::Sleep
     * @param useconds Time to sleep in microseconds
     */
    void Sleep(uint32_t useconds);

}

#endif // DMSDK_TIME_H
