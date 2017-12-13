#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <script/script.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "script_sprite.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}


namespace dmGameSystem
{
    /*# Sprite API documentation
     *
     * Functions, messages and properties used to manipulate sprite components.
     *
     * @document
     * @name Sprite
     * @namespace sprite
     */

    /*# [type:vector3] sprite size
     *
     * [mark:READ ONLY] Returns the size of the sprite, not allowing for any additional scaling that may be applied.
     * The type of the property is vector3.
     *
     * @name size
     * @property
     *
     * @examples
     *
     * How to query a sprite's size, either as a vector or selecting a specific dimension:
     *
     * ```lua
     * function init(self)
     *   -- get size from component "sprite"
     *   local size = go.get("#sprite", "size")
     *   local sx = go.get("#sprite", "size.x")
     *   -- do something useful
     *   assert(size.x == sx)
     * end
     * ```
     */

    /*# [type:vector3] sprite scale
     *
     * The non-uniform scale of the sprite. The type of the property is vector3.
     *
     * @name scale
     * @property
     *
     * @examples
     *
     * How to scale a sprite independently along the X and Y axis:
     *
     * ```lua
     * function init(self)
     *   -- Double the y-axis scaling on component "sprite"
     * 	 local yscale = go.get("#sprite", "scale.y")
     * 	 go.set("#sprite", "scale.y", yscale * 2)
     * end
     * ```
     */

    /*# [type:hash] sprite textureset
     *
     * The textureset hash id of the sprite. Used for getting/setting sprite textureset
     *
     * @name textureset
     * @property
     *
     * @examples
     *
     * How to set sprite textureset from a go textureset resource property
     *
     * ```lua
     * go.property("mytextureset", textureset("/main/texture.atlas")
     * function init(self)
     *   go.set("#sprite", "textureset", self.mytextureset)
     * end
     * ```
     */

    /*# [type:hash] sprite texture(n) where n is 0-7
     *
     * The texture hash id of the sprite. Used for getting/setting sprite texture for unit 0-7
     *
     * @name texture(n)
     * @property
     *
     * @examples
     *
     * How to set sprite texture for unit 0 from a go texture resource property
     *
     * ```lua
     * go.property("mytexture", texture("/main/texture.png")
     * function init(self)
     *   go.set("#sprite", "texture0", self.mytexture)
     * end
     * ```
     */

    /*# [type:hash] sprite material
     *
     * The material hash id of the sprite. Used for getting/setting sprite material
     *
     * @name material
     * @property
     *
     * @examples
     *
     * How to set sprite material from a go material resource property
     *
     * ```lua
     * go.property("mymaterial", material("/main/material.material")
     * function init(self)
     *   go.set("#sprite", "material", self.mymaterial)
     * end
     * ```
     */

    /*# set horizontal flipping on a sprite's animations
     * Sets horizontal flipping of the provided sprite's animations.
     * The sprite is identified by its URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_hflip
     * @param url [type:string|hash|url] the sprite that should flip its animations
     * @param flip [type:boolean] `true` if the sprite should flip its animations, `false` if not
     * @examples
     *
     * How to flip a sprite so it faces the horizontal movement:
     *
     * ```lua
     * function update(self, dt)
     *   -- calculate self.velocity somehow
     *   sprite.set_hflip("#sprite", self.velocity.x < 0)
     * end
     * ```
     *
     * It is assumed that the sprite component has id "sprite" and that the original animations faces right.
     */
    int SpriteComp_SetHFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::SetFlipHorizontal msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set vertical flipping on a sprite's animations
     * Sets vertical flipping of the provided sprite's animations.
     * The sprite is identified by its URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_vflip
     * @param url [type:string|hash|url] the sprite that should flip its animations
     * @param flip [type:boolean] `true` if the sprite should flip its animations, `false` if not
     * @examples
     *
     * How to flip a sprite in a game which negates gravity as a game mechanic:
     *
     * ```lua
     * function update(self, dt)
     *   -- calculate self.up_side_down somehow, then:
     *   sprite.set_vflip("#sprite", self.up_side_down)
     * end
     * ```
     *
     * It is assumed that the sprite component has id "sprite" and that the original animations are up-right.
     */
    int SpriteComp_SetVFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::SetFlipVertical msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a shader constant for a sprite
     * Sets a shader constant for a sprite component.
     * The constant must be defined in the material assigned to the sprite.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until `sprite.reset_constant` is called.
     * Which sprite to set a constant for is identified by the URL.
     *
     * @name sprite.set_constant
     * @param url [type:string|hash|url] the sprite that should have a constant set
     * @param constant [type:string|hash] name of the constant
     * @param value [type:vector4] of the constant
     * @examples
     *
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can set the constants defined there in the same manner.
     *
     * How to tint a sprite red:
     *
     * ```lua
     * function init(self)
     *   sprite.set_constant("#sprite", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    int SpriteComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstant msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a sprite
     * Resets a shader constant for a sprite component.
     * The constant must be defined in the material assigned to the sprite.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which sprite to reset a constant for is identified by the URL.
     *
     * @name sprite.reset_constant
     * @param url [type:string|hash|url] the sprite that should have a constant reset
     * @param constant [type:string|hash] name of the constant
     * @examples
     *
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of a sprite:
     *
     * ```lua
     * function init(self)
     *   sprite.reset_constant("#sprite", "tint")
     * end
     * ```
     */
    int SpriteComp_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstant msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    // Docs intentionally left out until we decide to go public with this function
    int SpriteComp_SetScale(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        Vectormath::Aos::Vector3* scale = dmScript::CheckVector3(L, 2);

        dmGameSystemDDF::SetScale msg;
        msg.m_Scale = *scale;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetScale::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg SPRITE_COMP_FUNCTIONS[] =
    {
            {"set_hflip",       SpriteComp_SetHFlip},
            {"set_vflip",       SpriteComp_SetVFlip},
            {"set_constant",    SpriteComp_SetConstant},
            {"reset_constant",  SpriteComp_ResetConstant},
            {"set_scale",       SpriteComp_SetScale},
            {0, 0}
    };

    void ScriptSpriteRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "sprite", SPRITE_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
