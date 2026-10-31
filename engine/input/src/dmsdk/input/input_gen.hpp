// Generated, do not edit!
// Generated from sdk_gen.json (C++ API).

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

#ifndef DMSDK_INPUT_GEN_HPP
#define DMSDK_INPUT_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Input action API
 *
 * Read resolved input actions delivered by engine input events. Batches and
 * actions are immutable and borrowed only during the synchronous event callback.
 * Do not retain handles or use them from another thread. Use batch accessors;
 * action storage has no public layout or stride.
 *
 * @document
 * @language C++
 * @name Input
 * @namespace dmInput
 * @path engine/input/src/dmsdk/input/input_gen.hpp
 */

#include <stdint.h>
#include <dmsdk/dlib/hash.h>

#include <dmsdk/input/input.h>

namespace dmInput
{
    /*# resolved action source
     * resolved action source
     * @enum
     * @name ActionSource
     * @language C++
     * @member  ACTION_SOURCE_UNKNOWN Unrecognized device category.
     * @member  ACTION_SOURCE_KEYBOARD Keyboard binding.
     * @member  ACTION_SOURCE_TEXT Text or marked-text binding.
     * @member  ACTION_SOURCE_MOUSE Mouse binding or pointer movement.
     * @member  ACTION_SOURCE_TOUCH Touch binding.
     * @member  ACTION_SOURCE_GAMEPAD Gamepad binding or connectivity notification.
     * @member  ACTION_SOURCE_ACCELEROMETER Accelerometer sample.
     */
    typedef ::InputActionSource ActionSource;
    static const ActionSource ACTION_SOURCE_UNKNOWN = ::INPUT_ACTION_SOURCE_UNKNOWN;
    static const ActionSource ACTION_SOURCE_KEYBOARD = ::INPUT_ACTION_SOURCE_KEYBOARD;
    static const ActionSource ACTION_SOURCE_TEXT = ::INPUT_ACTION_SOURCE_TEXT;
    static const ActionSource ACTION_SOURCE_MOUSE = ::INPUT_ACTION_SOURCE_MOUSE;
    static const ActionSource ACTION_SOURCE_TOUCH = ::INPUT_ACTION_SOURCE_TOUCH;
    static const ActionSource ACTION_SOURCE_GAMEPAD = ::INPUT_ACTION_SOURCE_GAMEPAD;
    static const ActionSource ACTION_SOURCE_ACCELEROMETER = ::INPUT_ACTION_SOURCE_ACCELEROMETER;

    /*# read-only resolved action
     * Borrowed from a batch for the duration of its engine event callback.
     * @typedef
     * @name HAction
     * @language C++
     */
    typedef HInputAction HAction;

    /*# read-only resolved action batch
     * One completed input update, including updates with no active actions.
     * Borrowed for the duration of its engine event callback.
     * @typedef
     * @name HActionBatch
     * @language C++
     */
    typedef HInputActionBatch HActionBatch;

    /*# count resolved actions
     * count resolved actions
     * @name ActionBatchGetCount
     * @language C++
     * @param batch [type:HActionBatch] valid borrowed batch
     * @return count [type:uint32_t] number of actions; zero for an empty update
     */
    uint32_t ActionBatchGetCount(HActionBatch batch);

    /*# read one resolved action
     * read one resolved action
     * @name ActionBatchGetAction
     * @language C++
     * @param batch [type:HActionBatch] valid borrowed batch
     * @param index [type:uint32_t] zero-based action index
     * @return action [type:HAction] borrowed action, or null when index is out of range
     */
    HAction ActionBatchGetAction(HActionBatch batch, uint32_t index);

    /*# read the action identifier
     * read the action identifier
     * @name ActionGetId
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
     */
    dmhash_t ActionGetId(HAction action);

    /*# read the scalar binding value
     * read the scalar binding value
     * @name ActionGetValue
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return value [type:float] resolved scalar value, including zero on release
     */
    float ActionGetValue(HAction action);

    /*# test the press transition
     * test the press transition
     * @name ActionIsPressed
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return pressed [type:bool] true if the binding became active this input update
     */
    bool ActionIsPressed(HAction action);

    /*# test the release transition
     * test the release transition
     * @name ActionIsReleased
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return released [type:bool] true if the binding became inactive this input update
     */
    bool ActionIsReleased(HAction action);

    /*# test binding repeat
     * test binding repeat
     * @name ActionIsRepeated
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return repeated [type:bool] true if repeat was triggered this input update
     */
    bool ActionIsRepeated(HAction action);

    /*# read the device category
     * read the device category
     * @name ActionGetSource
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @return source [type:ActionSource] device category, or ACTION_SOURCE_UNKNOWN
     */
    ActionSource ActionGetSource(HAction action);

    /*# read the gamepad index
     * read the gamepad index
     * @name ActionGetGamepadIndex
     * @language C++
     * @param action [type:HAction] valid borrowed action
     * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
     * @return result [type:bool] true if the action belongs to a gamepad
     */
    bool ActionGetGamepadIndex(HAction action, uint32_t* out_index);


} // namespace dmInput

#endif // #define DMSDK_INPUT_GEN_HPP
/*# resolved action source
 * resolved action source
 * @enum
 * @name InputActionSource
 * @language C
 * @member  INPUT_ACTION_SOURCE_UNKNOWN Unrecognized device category.
 * @member  INPUT_ACTION_SOURCE_KEYBOARD Keyboard binding.
 * @member  INPUT_ACTION_SOURCE_TEXT Text or marked-text binding.
 * @member  INPUT_ACTION_SOURCE_MOUSE Mouse binding or pointer movement.
 * @member  INPUT_ACTION_SOURCE_TOUCH Touch binding.
 * @member  INPUT_ACTION_SOURCE_GAMEPAD Gamepad binding or connectivity notification.
 * @member  INPUT_ACTION_SOURCE_ACCELEROMETER Accelerometer sample.
 */

/*# read-only resolved action
 * Borrowed from a batch for the duration of its engine event callback.
 * @typedef
 * @name HInputAction
 * @language C
 */

/*# read-only resolved action batch
 * One completed input update, including updates with no active actions.
 * Borrowed for the duration of its engine event callback.
 * @typedef
 * @name HInputActionBatch
 * @language C
 */

/*# count resolved actions
 * count resolved actions
 * @name InputActionBatchGetCount
 * @language C
 * @param batch [type:HInputActionBatch] valid borrowed batch
 * @return count [type:uint32_t] number of actions; zero for an empty update
 */

/*# read one resolved action
 * read one resolved action
 * @name InputActionBatchGetAction
 * @language C
 * @param batch [type:HInputActionBatch] valid borrowed batch
 * @param index [type:uint32_t] zero-based action index
 * @return action [type:HInputAction] borrowed action, or null when index is out of range
 */

/*# read the action identifier
 * read the action identifier
 * @name InputActionGetId
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
 */

/*# read the scalar binding value
 * read the scalar binding value
 * @name InputActionGetValue
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return value [type:float] resolved scalar value, including zero on release
 */

/*# test the press transition
 * test the press transition
 * @name InputActionIsPressed
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return pressed [type:bool] true if the binding became active this input update
 */

/*# test the release transition
 * test the release transition
 * @name InputActionIsReleased
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return released [type:bool] true if the binding became inactive this input update
 */

/*# test binding repeat
 * test binding repeat
 * @name InputActionIsRepeated
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return repeated [type:bool] true if repeat was triggered this input update
 */

/*# read the device category
 * read the device category
 * @name InputActionGetSource
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @return source [type:InputActionSource] device category, or INPUT_ACTION_SOURCE_UNKNOWN
 */

/*# read the gamepad index
 * read the gamepad index
 * @name InputActionGetGamepadIndex
 * @language C
 * @param action [type:HInputAction] valid borrowed action
 * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
 * @return result [type:bool] true if the action belongs to a gamepad
 */
