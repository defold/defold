// Copyright 2020-2026 The Defold Foundation
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

#ifndef DMSDK_SOUND_SOUND_H
#define DMSDK_SOUND_SOUND_H

#include <dmsdk/dlib/hash.h>

/*# Sound API documentation
 *
 * Functions for controlling the engine sound mixer from native extensions.
 *
 * @document
 * @name Sound
 * @namespace dmSound
 * @language C++
 */

namespace dmSound
{
    /*#
     * @enum
     * @name Result
     * @member RESULT_OK
     * @member RESULT_PARTIAL_DATA
     * @member RESULT_OUT_OF_SOURCES
     * @member RESULT_EFFECT_NOT_FOUND
     * @member RESULT_OUT_OF_INSTANCES
     * @member RESULT_RESOURCE_LEAK
     * @member RESULT_OUT_OF_BUFFERS
     * @member RESULT_INVALID_PROPERTY
     * @member RESULT_UNKNOWN_SOUND_TYPE
     * @member RESULT_INVALID_STREAM_DATA
     * @member RESULT_OUT_OF_MEMORY
     * @member RESULT_UNSUPPORTED
     * @member RESULT_DEVICE_NOT_FOUND
     * @member RESULT_OUT_OF_GROUPS
     * @member RESULT_NO_SUCH_GROUP
     * @member RESULT_NOTHING_TO_PLAY
     * @member RESULT_INIT_ERROR
     * @member RESULT_FINI_ERROR
     * @member RESULT_NO_DATA
     * @member RESULT_END_OF_STREAM
     * @member RESULT_DEVICE_LOST
     * @member RESULT_UNKNOWN_ERROR
     */
    enum Result
    {
        RESULT_OK                 =  0,
        RESULT_PARTIAL_DATA       =  1,
        RESULT_OUT_OF_SOURCES     = -1,
        RESULT_EFFECT_NOT_FOUND   = -2,
        RESULT_OUT_OF_INSTANCES   = -3,
        RESULT_RESOURCE_LEAK      = -4,
        RESULT_OUT_OF_BUFFERS     = -5,
        RESULT_INVALID_PROPERTY   = -6,
        RESULT_UNKNOWN_SOUND_TYPE = -7,
        RESULT_INVALID_STREAM_DATA= -8,
        RESULT_OUT_OF_MEMORY      = -9,
        RESULT_UNSUPPORTED        = -10,
        RESULT_DEVICE_NOT_FOUND   = -11,
        RESULT_OUT_OF_GROUPS      = -12,
        RESULT_NO_SUCH_GROUP      = -13,
        RESULT_NOTHING_TO_PLAY    = -14,
        RESULT_INIT_ERROR         = -15,
        RESULT_FINI_ERROR         = -16,
        RESULT_NO_DATA            = -17,
        RESULT_END_OF_STREAM      = -18,
        RESULT_DEVICE_LOST        = -19,
        RESULT_UNKNOWN_ERROR      = -1000,
    };

    /*# Set mute state for a mixer group
     * Temporarily mute or restore an individual mixer group.
     *
     * @name SetGroupMute
     * @param group [type:dmhash_t] hash of the mixer group (e.g. `hash("master")`)
     * @param mute [type:bool] `true` to mute, `false` to restore audio
     * @return result [type:Result] RESULT_OK on success
     */
    Result SetGroupMute(dmhash_t group, bool mute);

    /*# Toggle mute state for a mixer group
     * Convenience toggle for `SetGroupMute`.
     *
     * @name ToggleGroupMute
     * @param group [type:dmhash_t] hash of the mixer group (e.g. `hash("master")`)
     * @return result [type:Result] RESULT_OK on success
     */
    Result ToggleGroupMute(dmhash_t group);

    /*# Query group mute state
     * @name IsGroupMuted
     * @param group [type:dmhash_t] hash of the mixer group (e.g. `hash("master")`)
     * @return muted [type:bool] `true` if the mixer group is muted
     */
    bool IsGroupMuted(dmhash_t group);

}

#endif // DMSDK_SOUND_SOUND_H
