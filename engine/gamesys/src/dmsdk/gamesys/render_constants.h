// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RENDER_CONSTANTS_H
#define DMSDK_GAMESYS_RENDER_CONSTANTS_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/render/render.h>

/*# Component render constant API documentation
 * [file:<dmsdk/gamesystem/render_constants.h>]
 *
 * Api for setting and updating component render constants
 *
 * @document
 * @name Component Render Constants
 * @namespace dmGameSystem
 */

namespace dmGameObject
{
    struct PropertyVar;
}

namespace dmRender
{
    struct RenderObject;
    struct Constant;
}

namespace dmGameSystem
{
    /*#
     * Render constants handle
     * @typedef
     * @name HComponentRenderConstants
     */
    typedef struct CompRenderConstants* HComponentRenderConstants;

    /*#
     * Create a new HComponentRenderConstants container
     * @name CreateRenderConstants
     * @return constants [type: dmGameSystem::HComponentRenderConstants]
     */
    HComponentRenderConstants CreateRenderConstants();

    /*#
     * Destroys a render constants container
     * @name DestroyRenderConstants
     * @param constants [type: dmGameSystem::HComponentRenderConstants] (must not be 0)
     */
    void DestroyRenderConstants(HComponentRenderConstants constants);

    /*#
     * Destroys a render constants container
     * @name GetRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param name_hash [type: dmhash_t] the hashed name of the property
     * @param out_constant [type: dmRender::Constant**] the pointer where to store the constant
     * @return result [type: bool] returns true if the constant exists
     */
    bool GetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmRender::HConstant* out_constant);

    /*#
     * Get the number of render constants
     * @name GetRenderConstantCount
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @return size [type: uint32_t] returns the number of set constants
     */
    uint32_t GetRenderConstantCount(HComponentRenderConstants constants);

    /*#
     * Get a render constant by index
     * @name GetRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param index [type: uint32_t] the index
     * @return constant [type: dmRender::HConstant] the pointer where to store the constant
     */
    dmRender::HConstant GetRenderConstant(HComponentRenderConstants constants, uint32_t index);

    /*#
     * Set a render constant by name. The constant must exist in the material
     * @name SetRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the render constants buffer
     * @param material [type: dmRender::HMaterial] the material to get default values from if constant didn't already exist in the render constants buffer
     * @param name_hash [type: dmhash_t] the hashed name of the constant
     * @param value_index [type: uint32_t] index of the constant value to set, if the constant is an array
     * @param element_index [type: uint32_t*] pointer to the index of the element (in range [0,3]). May be 0
     * @param var [type: const dmGameObject::PropertyVar&] the constant value
     */
    void SetRenderConstant(HComponentRenderConstants constants, dmRender::HMaterial material, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var);

    /*#
     * Set a render constant by name. The constant must exist in the material
     * @name SetRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param name_hash [type: dmhash_t] the hashed name of the constant
     * @param values [type: dmVMath::Vector4*] the values
     * @param num_values [type: uint32_t] number of values in the array
     */
    void SetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values);

    /*#
     * Removes a render constant from the container
     * @name ClearRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param name_hash [type: dmhash_t] the hashed name of the constant
     * @return result [type: int] non zero if the constant was removed
     */
    int ClearRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash);

    /*#
     * Hashes the constants
     * @note Also updates the internal state of the constants container. After a call to this function, the `AreRenderConstantsUpdated` will always return false.
     * @name HashRenderConstants
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param state [type: HashState32*] the hash state to update
     */
    void HashRenderConstants(HComponentRenderConstants constants, HashState32* state);

    /*# check if the constants have changed
     * @name AreRenderConstantsUpdated
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @return result [type: int] non zero if the constants were changed
     */
    int AreRenderConstantsUpdated(HComponentRenderConstants constants);

    /*# set the constants of a render object
     * @name EnableRenderObjectConstants
     * @param ro [type: dmRender::RenderObject*] the render object
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     */
    void EnableRenderObjectConstants(dmRender::RenderObject* ro, HComponentRenderConstants constants);

    /*#
     * Used in GetMaterialConstant to resolve a render constant's value
     * @typedef
     * @name CompGetConstantCallback
     */
    typedef bool (*CompGetConstantCallback)(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant);

    /**
     * Helper function to get material constants of components that use them: sprite, label, tile maps, spine and models
     * Sprite and Label should not use value ptr. Deleting a gameobject (that included sprite(s) or label(s)) will rearrange the
     * object pool for components (due to EraseSwap in the Free for the object pool). This result in the original animation value pointer will still point
     * to the original memory location in the component object pool.
     * @name GetMaterialConstant
     * @param material [type: dmRender::HMaterial] the material
     * @param name_hash [type: dmhash_t] the name of the property
     * @param value_index [type: int32_t] the index of the constant value to get, if it is an array
     * @param out_desc [type: dmGameObject::PropertyDesc&] the property descriptor
     * @param use_value_ptr [type: bool] should the property pointer be used (m_ValuePtr)
     * @param callback [type: CompGetConstantCallback] callback to resolve property
     * @param callback_user_data [type: void*] callback user data
     * @return result [type: dmGameObject::PropertyResult] the result
     */
    dmGameObject::PropertyResult GetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, int32_t value_index, dmGameObject::PropertyDesc& out_desc, bool use_value_ptr, CompGetConstantCallback callback, void* callback_user_data);

    /*#
     * Used in SetMaterialConstant to set a render constant's value
     * @typedef
     * @name CompSetConstantCallback
     */
    typedef void (*CompSetConstantCallback)(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var);

    /**
     * Helper function to set material constants of components that use them: sprite, label, tile maps, spine and models
     * @name SetMaterialConstant
     * @param material [type: dmRender::HMaterial] the material
     * @param name_hash [type: dmhash_t] the name of the property
     * @param value_index [type: uint32_t] index of the constant value to set, if the material constant is an array
     * @param var [type: dmGameObject::PropertyVar] the property
     * @param callback [type: CompGetConstantCallback] the callback used to set the property
     * @param callback_user_data [type: void*] callback user data
     * @return result [type: dmGameObject::PropertyResult] the result
     */
    dmGameObject::PropertyResult SetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var, int32_t value_index, CompSetConstantCallback callback, void* callback_user_data);

}

#endif // DMSDK_GAMESYS_RENDER_CONSTANTS_H
