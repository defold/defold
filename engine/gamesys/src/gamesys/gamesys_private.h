// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_GAMESYS_PRIVER_H
#define DM_GAMESYS_PRIVER_H

#include <dlib/message.h>

#include <render/render.h>

#include <gameobject/gameobject.h>

namespace dmScript
{
    struct LuaCallbackInfo;
}

namespace dmGameSystem
{
#define EXT_CONSTANTS(prefix, ext)\
    static const char* prefix##_EXT = ext;\
    static const dmhash_t prefix##_EXT_HASH = dmHashString64(ext);\

    EXT_CONSTANTS(COLLECTION_FACTORY, "collectionfactoryc")
    EXT_CONSTANTS(COLLISION_OBJECT, "collisionobjectc")
    EXT_CONSTANTS(FACTORY, "factoryc")
    EXT_CONSTANTS(FONT, "fontc")
    EXT_CONSTANTS(MATERIAL, "materialc")
    EXT_CONSTANTS(BUFFER, "bufferc")
    EXT_CONSTANTS(MODEL, "modelc")
    EXT_CONSTANTS(TEXTURE, "texturec")
    EXT_CONSTANTS(TEXTURE_SET, "texturesetc")
    EXT_CONSTANTS(TILE_MAP, "tilemapc")

#undef EXT_CONSTANTS

    static const dmhash_t PROP_FONT = dmHashString64("font");
    static const dmhash_t PROP_FONTS = dmHashString64("fonts");
    static const dmhash_t PROP_IMAGE = dmHashString64("image");
    static const dmhash_t PROP_MATERIAL = dmHashString64("material");
    static const dmhash_t PROP_MATERIALS = dmHashString64("materials");
    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"),
        dmHashString64("texture1"),
        dmHashString64("texture2"),
        dmHashString64("texture3"),
        dmHashString64("texture4"),
        dmHashString64("texture5"),
        dmHashString64("texture6"),
        dmHashString64("texture7")
    };
    static const dmhash_t PROP_TEXTURES = dmHashString64("textures");
    static const dmhash_t PROP_TILE_SOURCE = dmHashString64("tile_source");

    struct EmitterStateChangedScriptData
    {
        EmitterStateChangedScriptData()
        {
            memset(this, 0, sizeof(*this));
        }

        dmhash_t m_ComponentId;
        dmScript::LuaCallbackInfo* m_CallbackInfo;
    };

    // A wrapper for dmScript::CheckGoInstance
    dmGameObject::HInstance CheckGoInstance(lua_State* L);

    // Logs an error for when a component buffer is full
    void ShowFullBufferError(const char* object_name, const char* config_key, int max_count);

    // Logs an error for when a component buffer is full, but the buffer in question cannot be changed by the user
    void ShowFullBufferError(const char* object_name, int max_count);

    /**
     * Log message error. The function will send a formatted printf-style string to dmLogError
     * and append message sender/receiver information on the following format:
     * Message <MESSAGE-ID> sent from <SENDER> to <RECEIVER>. For format-string should be a complete sentence including
     * a terminating period character but without trailing space.
     * @param message message
     * @param format printf-style format string
     */
    void LogMessageError(dmMessage::Message* message, const char* format, ...);
}

#endif // DM_GAMESYS_PRIVER_H
