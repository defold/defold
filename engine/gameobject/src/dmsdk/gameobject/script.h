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

#ifndef DMSDK_GAMEOBJECT_SCRIPT_H
#define DMSDK_GAMEOBJECT_SCRIPT_H

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

#include <dmsdk/dlib/message.h>
#include <dmsdk/ddf/ddf.h>
#include <dmsdk/gameobject/gameobject.h>

namespace dmMessage {
    struct URL;
}

namespace dmGameObject
{
    /*# SDK GameObject script API documentation
     *
     * @document
     * @name Script
     * @namespace dmGameObject
     * @language C++
     */

    Result PostScriptMessage(const dmDDF::Descriptor* descriptor, const uint8_t* payload, uint32_t payload_size, const dmMessage::URL* sender, const dmMessage::URL* receiver, int function_ref, bool unref_function_after_call);

    /*# Sends an unref script message
     * Sends a script message to unreference a script object
     *
     * @name PostScriptUnrefMessage
     * @param sender [type:dmMessage::Message*] The sender
     * @param receiver [type:dmMessage::Message*] The receiver
     * @param reference [type:int] The reference to remove
     * @return success [type:Result] RESULT_OK if successful
     */
    Result PostScriptUnrefMessage(const dmMessage::URL* sender, const dmMessage::URL* receiver, int reference);

    /*# Sends a script message
     * Sends a script message. Wraps the message in a dmGameSystemDDF::ScriptMessage struct.
     * @name PostDDF
     * @tparam T
     * @param message [type:T*] The ddf message to send
     * @param sender [type:dmMessage::Message*] The sender
     * @param receiver [type:dmMessage::Message*] The receiver
     * @param function_ref [type:int] The function ref. 0 wil cause the "on_message" to be called
     * @param unref_function_after_call [type:bool] call dmScript::UnrefInInstance on the function_ref after the dmScript::PCall is made
     * @return success [type:bool] true if successful
     */
    template <typename T>
    Result PostDDF(const T* message, const dmMessage::URL* sender, const dmMessage::URL* receiver, int function_ref, bool unref_function_after_call)
    {
        dmArray<uint8_t> packed_message;
        packed_message.SetCapacity(sizeof(T)); // A good estimate for most messages
        dmDDF::SaveMessageToArray(message, message->m_DDFDescriptor, packed_message);
        return PostScriptMessage(message->m_DDFDescriptor, packed_message.Begin(), packed_message.Size(), sender, receiver, function_ref, unref_function_after_call);
    }
}

namespace dmScript
{
    /*#
     * Creates a property container from a lua table
     * @name PropertyContainerCreateFromLua
     * @param L [type: lua_State*] The lua state
     * @param index [type: int] The lua stack index of the lua table
     * @return container [type: HPropertyContainer] The property container
     */
    dmGameObject::HPropertyContainer PropertyContainerCreateFromLua(lua_State* L, int index);
}

#endif // DMSDK_GAMEOBJECT_SCRIPT_H
