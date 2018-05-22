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
#define SPINE_MODEL_EXT "spinemodelc"
#define MODEL_EXT "modelc"
#define TILE_MAP_EXT "tilegridc"
#define FACTORY_EXT "factoryc"
#define COLLECTION_FACTORY_EXT "collectionfactoryc"
#define COLLISION_OBJECT_EXT "collisionobjectc"

    struct EmitterStateChangedScriptData
    {
        EmitterStateChangedScriptData()
        {
            memset(this, 0, sizeof(*this));
        }

        dmhash_t m_ComponentId;
        dmScript::LuaCallbackInfo* m_CallbackInfo;
    };

    /**
     * Return current game object instance, if any.
     * Must be called from within a lua pcall, since it long jumps if no instance can be found.
     * @param L lua state
     * @return instance
     */
    dmGameObject::HInstance CheckGoInstance(lua_State* L);

    /**
     * Log message error. The function will send a formatted printf-style string to dmLogError
     * and append message sender/receiver information on the following format:
     * Message <MESSAGE-ID> sent from <SENDER> to <RECEIVER>. For format-string should be a complete sentence including
     * a terminating period character but without trailing space.
     * @param message message
     * @param format printf-style format string
     */
    void LogMessageError(dmMessage::Message* message, const char* format, ...);

    typedef bool (*CompGetConstantCallback)(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant);

    /**
     * Helper function to get material constants of components that use them: sprite, label, tile maps, spine and models
     * Sprite and Label should not use value ptr. Deleting a gameobject (that included sprite(s) or label(s)) will rearrange the
     * object pool for components (due to EraseSwap in the Free for the object pool). This result in the original animation value pointer will still point
     * to the original memory location in the component object pool.
     */
    dmGameObject::PropertyResult GetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, dmGameObject::PropertyDesc& out_desc, bool use_value_ptr, CompGetConstantCallback callback, void* callback_user_data);

    typedef void (*CompSetConstantCallback)(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var);

    /**
     * Helper function to set material constants of components that use them: sprite, label, tile maps, spine and models
     */
    dmGameObject::PropertyResult SetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var, CompSetConstantCallback callback, void* callback_user_data);


}

#endif // DM_GAMESYS_PRIVER_H
