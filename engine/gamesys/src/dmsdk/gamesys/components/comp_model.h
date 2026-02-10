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

#ifndef DMSDK_GAMESYS_MODEL_H
#define DMSDK_GAMESYS_MODEL_H

#include <dmsdk/dlib/message.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/rig/rig.h>

/*# Model component functions
 *
 * API for interacting with model components.
 *
 * @document
 * @name Model
 * @namespace dmGameSystem
 * @language C++
 */

namespace dmGameSystem
{

    typedef struct ModelWorld* HModelWorld;
    typedef struct ModelComponent* HModelComponent;


    typedef void (*FModelAnimationCallback)(void* user_ctx, dmRig::RigEventType event, void* event_data);

    /*# 
     * Play a model animation.
     * @name CompModelPlayAnimation
     * @param world [type: HModelWorld] Model world.
     * @param component [type: HModelComponent] Model component.
     * @param anim_id [type: dmhash_t] Animation to play.
     * @param playback [type: dmRig::RigPlayback] Playback mode.
     * @param blend_duration [type: float] Duration of a linear blend between the current and new animation.
     * @param offset [type: float] The normalized initial value of the animation cursor when the animation starts playing.
     * @param playback_rate [type: float] The rate with which the animation will be played. Must be positive.
     * @param listener [type: dmMessage::URL] Optional URL to send play events to.
     * @param callback [type: FModelAnimationCallback] Optional function callback to send play events to.
     * @param callback_ctx [type: void*] Optional function callback context (only when callback is set)
     * @return result [type: dmRig::Result] Result of the operation.
     */
    dmRig::Result CompModelPlayAnimation(HModelWorld world, HModelComponent component, dmhash_t anim_id, dmRig::RigPlayback playback, float blend_duration, float offset, float playback_rate, dmMessage::URL listener, FModelAnimationCallback callback, void* callback_ctx);
}

#endif // DMSDK_GAMESYS_MODEL_H
