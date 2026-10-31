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

/*# Input action API
 * Iterate the resolved actions published during an engine input event. Action
 * storage is private; getters read the iterator's current action. Getters require
 * the last InputActionIterateNext call to have returned true.
 *
 * Call only on the engine thread. Iterators and current actions are borrowed
 * for the synchronous input callback; do not retain them or use another thread.
 * Iteration is read-only, allocation-free and independent between listeners.
 * @document
 * @name Input
 * @language C
 */

/*# input context handle
 * Borrowed engine-owned input context. Do not destroy it. The resolved action
 * view is available during its engine input event, including empty updates.
 * @typedef
 * @name HInputContext
 */
#if defined(__cplusplus)
namespace dmInput
{
    struct Context;
}
typedef dmInput::Context* HInputContext;
#else
typedef struct dmInputContext* HInputContext;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

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

/*# input action iterator
 * Small value-type cursor with reserved opaque state. Do not access its members;
 * initialize it with InputIterateActions and advance with InputActionIterateNext.
 * Its ABI occupies two uintptr_t words and requires no allocation or destruction.
 * @struct
 * @name InputActionIterator
 */
typedef struct InputActionIterator
{
    uintptr_t m_State[2];
} InputActionIterator;

/*# iterate resolved input actions
 * Captures the current event's immutable action view in dispatch order. The
 * iterator starts before the first action. New iterations outside input event
 * delivery are empty. Multiple iterators advance independently.
 * @name InputIterateActions
 * @param context [type:HInputContext] live borrowed input context
 * @return iterator [type:InputActionIterator] cursor positioned before the first action
 */
InputActionIterator InputIterateActions(HInputContext context);

/*# advance to the next resolved action
 * A false result invalidates the current action. Further advances remain false.
 * @name InputActionIterateNext
 * @param iterator [type:InputActionIterator*] iterator initialized by InputIterateActions
 * @return result [type:bool] true when positioned on an action; false when exhausted or empty
 */
bool InputActionIterateNext(InputActionIterator* iterator);

/*# read the action identifier
 * @name InputActionGetId
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return id [type:dmhash_t] hashed binding name; zero for unnamed pointer or acceleration samples
 */
dmhash_t InputActionGetId(const InputActionIterator* iterator);

/*# read the scalar binding value
 * @name InputActionGetValue
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return value [type:float] resolved scalar value, including zero on release
 */
float InputActionGetValue(const InputActionIterator* iterator);

/*# test the press transition
 * @name InputActionIsPressed
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return pressed [type:bool] true if the binding became active this input update
 */
bool InputActionIsPressed(const InputActionIterator* iterator);

/*# test the release transition
 * @name InputActionIsReleased
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return released [type:bool] true if the binding became inactive this input update
 */
bool InputActionIsReleased(const InputActionIterator* iterator);

/*# test binding repeat
 * @name InputActionIsRepeated
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return repeated [type:bool] true if repeat was triggered this input update
 */
bool InputActionIsRepeated(const InputActionIterator* iterator);

/*# read the device category
 * @name InputActionGetSource
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @return source [type:InputActionSource] device category, or INPUT_ACTION_SOURCE_UNKNOWN
 */
InputActionSource InputActionGetSource(const InputActionIterator* iterator);

/*# read the gamepad index
 * @name InputActionGetGamepadIndex
 * @param iterator [type:const InputActionIterator*] iterator positioned by a successful InputActionIterateNext
 * @param out_index [type:uint32_t*] non-null output; unchanged for a non-gamepad action
 * @return result [type:bool] true if the current action belongs to a gamepad
 */
bool InputActionGetGamepadIndex(const InputActionIterator* iterator, uint32_t* out_index);

#if defined(__cplusplus)
}
#include "input.hpp"
#endif

#endif // DMSDK_INPUT_H
