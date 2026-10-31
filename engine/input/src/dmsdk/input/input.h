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

#ifndef DMSDK_INPUT_H
#define DMSDK_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <dmsdk/dlib/hash.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*# Input action API
 * Read resolved input actions delivered by engine input events. Batches and
 * actions are immutable and borrowed only during the synchronous event callback.
 * Do not retain handles or use them from another thread. Use batch accessors;
 * action storage has no public layout or stride.
 * @document
 * @name Input
 * @language C
 */

/*# resolved action source
 * @enum
 * @name InputActionSource
 * @member INPUT_ACTION_SOURCE_UNKNOWN Unrecognized device category.
 * @member INPUT_ACTION_SOURCE_KEYBOARD Keyboard binding.
 * @member INPUT_ACTION_SOURCE_TEXT Text or marked-text binding.
 * @member INPUT_ACTION_SOURCE_MOUSE Mouse binding or pointer movement.
 * @member INPUT_ACTION_SOURCE_TOUCH Touch binding.
 * @member INPUT_ACTION_SOURCE_GAMEPAD Gamepad binding or connectivity notification.
 * @member INPUT_ACTION_SOURCE_ACCELEROMETER Accelerometer sample.
 */
typedef enum InputActionSource
{
    INPUT_ACTION_SOURCE_UNKNOWN       = -1,
    INPUT_ACTION_SOURCE_KEYBOARD      = 0,
    INPUT_ACTION_SOURCE_TEXT          = 1,
    INPUT_ACTION_SOURCE_MOUSE         = 2,
    INPUT_ACTION_SOURCE_TOUCH         = 3,
    INPUT_ACTION_SOURCE_GAMEPAD       = 4,
    INPUT_ACTION_SOURCE_ACCELEROMETER = 5,
} InputActionSource;

/*# read-only resolved action
 * Borrowed from a batch for the duration of its engine event callback.
 * @typedef
 * @name HInputAction
 */
typedef const struct dmInputAction* HInputAction;

/*# read-only resolved action batch
 * One completed input update, including updates with no active actions.
 * Borrowed for the duration of its engine event callback.
 * @typedef
 * @name HInputActionBatch
 */
typedef const struct dmInputActionBatch* HInputActionBatch;

/*# count resolved actions
 * @name InputActionBatchGetCount
 * @param batch [type:HInputActionBatch] valid borrowed batch
 * @return count [type:uint32_t] number of actions; zero for an empty update
 */
uint32_t InputActionBatchGetCount(HInputActionBatch batch);

/*# read one resolved action
 * @name InputActionBatchGetAction
 * @param batch [type:HInputActionBatch] valid borrowed batch
 * @param index [type:uint32_t] zero-based action index
 * @return action [type:HInputAction] borrowed action, or null when index is out of range
 */
HInputAction InputActionBatchGetAction(HInputActionBatch batch, uint32_t index);

/*# read the action identifier
 * @name InputActionGetId
 * @param action [type:HInputAction] valid borrowed action
 * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
 */
dmhash_t InputActionGetId(HInputAction action);

/*# read the scalar binding value
 * @name InputActionGetValue
 * @param action [type:HInputAction] valid borrowed action
 * @return value [type:float] resolved scalar value, including zero on release
 */
float InputActionGetValue(HInputAction action);

/*# test the press transition
 * @name InputActionIsPressed
 * @param action [type:HInputAction] valid borrowed action
 * @return pressed [type:bool] true if the binding became active this input update
 */
bool InputActionIsPressed(HInputAction action);

/*# test the release transition
 * @name InputActionIsReleased
 * @param action [type:HInputAction] valid borrowed action
 * @return released [type:bool] true if the binding became inactive this input update
 */
bool InputActionIsReleased(HInputAction action);

/*# test binding repeat
 * @name InputActionIsRepeated
 * @param action [type:HInputAction] valid borrowed action
 * @return repeated [type:bool] true if repeat was triggered this input update
 */
bool InputActionIsRepeated(HInputAction action);

/*# read the device category
 * @name InputActionGetSource
 * @param action [type:HInputAction] valid borrowed action
 * @return source [type:InputActionSource] device category, or INPUT_ACTION_SOURCE_UNKNOWN
 */
InputActionSource InputActionGetSource(HInputAction action);

/*# read the gamepad index
 * @name InputActionGetGamepadIndex
 * @param action [type:HInputAction] valid borrowed action
 * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
 * @return result [type:bool] true if the action belongs to a gamepad
 */
bool InputActionGetGamepadIndex(HInputAction action, uint32_t* out_index);

#if defined(__cplusplus)
}
#include "input.hpp"
#endif

#endif // DMSDK_INPUT_H
