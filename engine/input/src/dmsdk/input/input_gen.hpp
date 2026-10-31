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
 * Iterate the resolved actions published during an engine input event. Action
 * storage is private; getters read the iterator's current action. Getters require
 * the last ActionIterateNext call to have returned true.
 *
 * Call only on the engine thread. Iterators and current actions are borrowed
 * for the synchronous input callback; do not retain them or use another thread.
 * Iteration is read-only, allocation-free and independent between listeners.
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

    /*# input context handle
     * Borrowed engine-owned input context. Do not destroy it. The resolved action
     * view is available during its engine input event, including empty updates.
     * @typedef
     * @name HContext
     * @language C++
     */
    typedef HInputContext HContext;

    /*# input action iterator
     * Small value-type cursor with reserved opaque state. Do not access its members;
     * initialize it with IterateActions and advance with ActionIterateNext.
     * Its ABI occupies two uintptr_t words and requires no allocation or destruction.
     * @struct
     * @name ActionIterator
     * @language C++
     */
    typedef InputActionIterator ActionIterator;

    /*# iterate resolved input actions
     * Captures the current event's immutable action view in dispatch order. The
     * iterator starts before the first action. New iterations outside input event
     * delivery are empty. Multiple iterators advance independently.
     * @name IterateActions
     * @language C++
     * @param context [type:HContext] live borrowed input context
     * @return iterator [type:ActionIterator] cursor positioned before the first action
     */
    ActionIterator IterateActions(HContext context);

    /*# advance to the next resolved action
     * A false result invalidates the current action. Further advances remain false.
     * @name ActionIterateNext
     * @language C++
     * @param iterator [type:ActionIterator*] iterator initialized by IterateActions
     * @return result [type:bool] true when positioned on an action; false when exhausted or empty
     */
    bool ActionIterateNext(ActionIterator* iterator);

    /*# read the action identifier
     * read the action identifier
     * @name ActionGetId
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
     */
    dmhash_t ActionGetId(const ActionIterator* iterator);

    /*# read the scalar binding value
     * read the scalar binding value
     * @name ActionGetValue
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return value [type:float] resolved scalar value, including zero on release
     */
    float ActionGetValue(const ActionIterator* iterator);

    /*# test the press transition
     * test the press transition
     * @name ActionIsPressed
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return pressed [type:bool] true if the binding became active this input update
     */
    bool ActionIsPressed(const ActionIterator* iterator);

    /*# test the release transition
     * test the release transition
     * @name ActionIsReleased
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return released [type:bool] true if the binding became inactive this input update
     */
    bool ActionIsReleased(const ActionIterator* iterator);

    /*# test binding repeat
     * test binding repeat
     * @name ActionIsRepeated
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return repeated [type:bool] true if repeat was triggered this input update
     */
    bool ActionIsRepeated(const ActionIterator* iterator);

    /*# read the device category
     * read the device category
     * @name ActionGetSource
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @return source [type:ActionSource] device category, or ACTION_SOURCE_UNKNOWN
     */
    ActionSource ActionGetSource(const ActionIterator* iterator);

    /*# read the gamepad index
     * read the gamepad index
     * @name ActionGetGamepadIndex
     * @language C++
     * @param iterator [type:const ActionIterator*] iterator positioned by a successful ActionIterateNext
     * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
     * @return result [type:bool] true if the current action belongs to a gamepad
     */
    bool ActionGetGamepadIndex(const ActionIterator* iterator, uint32_t* out_index);


} // namespace dmInput

#endif // #define DMSDK_INPUT_GEN_HPP
/*# input context handle
 * Borrowed engine-owned input context. Do not destroy it. The resolved action
 * view is available during its engine input event, including empty updates.
 * @typedef
 * @name HInputContext
 * @language C
 */

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

/*# input action iterator
 * Small value-type cursor with reserved opaque state. Do not access its members;
 * initialize it with InputIterateActions and advance with InputActionIterateNext.
 * Its ABI occupies two uintptr_t words and requires no allocation or destruction.
 * @struct
 * @name InputActionIterator
 * @language C
 */

/*# iterate resolved input actions
 * Captures the current event's immutable action view in dispatch order. The
 * iterator starts before the first action. New iterations outside input event
 * delivery are empty. Multiple iterators advance independently.
 * @name InputIterateActions
 * @language C
 * @param context [type:HInputContext] live borrowed input context
 * @return iterator [type:InputActionIterator] cursor positioned before the first action
 */

/*# advance to the next resolved action
 * A false result invalidates the current action. Further advances remain false.
 * @name InputActionIterateNext
 * @language C
 * @param iterator [type:InputActionIterator*] iterator initialized by InputIterateActions
 * @return result [type:bool] true when positioned on an action; false when exhausted or empty
 */

/*# read the action identifier
 * read the action identifier
 * @name InputActionGetId
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
 */

/*# read the scalar binding value
 * read the scalar binding value
 * @name InputActionGetValue
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return value [type:float] resolved scalar value, including zero on release
 */

/*# test the press transition
 * test the press transition
 * @name InputActionIsPressed
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return pressed [type:bool] true if the binding became active this input update
 */

/*# test the release transition
 * test the release transition
 * @name InputActionIsReleased
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return released [type:bool] true if the binding became inactive this input update
 */

/*# test binding repeat
 * test binding repeat
 * @name InputActionIsRepeated
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return repeated [type:bool] true if repeat was triggered this input update
 */

/*# read the device category
 * read the device category
 * @name InputActionGetSource
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return source [type:InputActionSource] device category, or INPUT_ACTION_SOURCE_UNKNOWN
 */

/*# read the gamepad index
 * read the gamepad index
 * @name InputActionGetGamepadIndex
 * @language C
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
 * @return result [type:bool] true if the current action belongs to a gamepad
 */
