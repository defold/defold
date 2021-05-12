// Copyright 2020 The Defold Foundation
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
    bool GetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmRender::Constant** out_constant);

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
     * @param constant [type: dmRender::Constant*] the pointer where to store the constant
     */
    void GetRenderConstant(HComponentRenderConstants constants, uint32_t index, dmRender::Constant* constant);

    /*#
     * Set a render constant by name. The constant must exist in the material
     * @name SetRenderConstant
     * @param constants [type: dmGameSystem::HComponentRenderConstants] the constants
     * @param material [type: dmRender::HMaterial] the material
     * @param name_hash [type: dmhash_t] the hashed name of the constant
     * @param element_index [type: uint32_t*] pointer to the index of the element (in range [0,3]). May be 0
     * @param var [type: const dmGameObject::PropertyVar&] the constant value
     */
    void SetRenderConstant(HComponentRenderConstants constants, dmRender::HMaterial material, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var);

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
}

#endif // DMSDK_GAMESYS_RENDER_CONSTANTS_H
