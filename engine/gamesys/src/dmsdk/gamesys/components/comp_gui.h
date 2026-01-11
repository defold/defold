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

#ifndef DMSDK_GAMESYSTEM_COMP_GUI_H
#define DMSDK_GAMESYSTEM_COMP_GUI_H

#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gameobject/component.h>
#include <dmsdk/gui/gui.h>

/*# SDK Gui Component Property API documentation
 *
 * Per-property registration functions for GUI component extensions.
 *
 * @document
 * @name GameSystem GUI Component
 * @namespace dmGameSystem
 * @path engine/gamesys/src/dmsdk/gamesys/components/comp_gui.h
 * @language C++
 */

namespace dmGameSystem
{
    /*#
     * GUI component property setter function
     * @typedef CompGuiPropertySetterFn
     * @param scene [type:dmGui::HScene] The GUI scene
     * @param params [type:dmGameObject::ComponentSetPropertyParams] Property parameters
     * @return [type:dmGameObject::PropertyResult] PROPERTY_RESULT_OK on success
     */
    typedef dmGameObject::PropertyResult (*CompGuiPropertySetterFn)(dmGui::HScene scene, const dmGameObject::ComponentSetPropertyParams& params);

    /*#
     * GUI component property getter function
     * @typedef CompGuiPropertyGetterFn
     * @param scene [type:dmGui::HScene] The GUI scene
     * @param params [type:dmGameObject::ComponentGetPropertyParams] Property parameters
     * @param out_value [type:dmGameObject::PropertyDesc&] Output value
     * @return [type:dmGameObject::PropertyResult] PROPERTY_RESULT_OK on success
     */
    typedef dmGameObject::PropertyResult (*CompGuiPropertyGetterFn)(dmGui::HScene scene, const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    /**
     * Register a GUI component property setter for a specific property
     * @param property_hash [type:dmhash_t] Hash of the property name
     * @param setter [type:CompGuiPropertySetterFn] Setter function
     * @return [type:dmGameObject::Result] RESULT_OK on success
     */
    dmGameObject::Result CompGuiRegisterSetPropertyFn(dmhash_t property_hash, CompGuiPropertySetterFn setter);

    /**
     * Register a GUI component property getter for a specific property
     * @param property_hash [type:dmhash_t] Hash of the property name
     * @param getter [type:CompGuiPropertyGetterFn] Getter function
     * @return [type:dmGameObject::Result] RESULT_OK on success
     */
    dmGameObject::Result CompGuiRegisterGetPropertyFn(dmhash_t property_hash, CompGuiPropertyGetterFn getter);

    /**
     * Unregister a GUI component property setter for a specific property
     * @param property_hash [type:dmhash_t] Hash of the property name
     * @return [type:dmGameObject::Result] RESULT_OK on success, RESULT_UNKNOWN_ERROR if not found
     */
    dmGameObject::Result CompGuiUnregisterSetPropertyFn(dmhash_t property_hash);

    /**
     * Unregister a GUI component property getter for a specific property
     * @param property_hash [type:dmhash_t] Hash of the property name
     * @return [type:dmGameObject::Result] RESULT_OK on success, RESULT_UNKNOWN_ERROR if not found
     */
    dmGameObject::Result CompGuiUnregisterGetPropertyFn(dmhash_t property_hash);
}

#endif // DMSDK_GAMESYSTEM_COMP_GUI_H