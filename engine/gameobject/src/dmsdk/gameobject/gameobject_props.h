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

#ifndef DMSDK_GAMEOBJECT_PROPS_H
#define DMSDK_GAMEOBJECT_PROPS_H

/*# Game object property container
 *
 * API for game object property container
 *
 * @document
 * @name PropertyContainer
 * @namespace dmGameObject
 * @path engine/gameobject/src/dmsdk/gameobject/gameobject_props.h
 */

#include <stdint.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/message.h>
#include <dmsdk/resource/resource.h>

namespace dmGameObject
{
     /*#
     * Opaque handle to a list of properties
     * @typedef
     * @name HPropertyContainer
     */
    typedef struct PropertyContainer* HPropertyContainer;

     /*#
     * Opaque handle to a property container builder
     * @typedef
     * @name HPropertyContainerBuilder
     */
    typedef struct PropertyContainerBuilder* HPropertyContainerBuilder;

    /*# PropertyContainerBuilderParams
     *
     * Helper struct to create a property container builder.
     * It is required to fill out how many items of each type that is wanted.
     *
     * @struct
     * @name PropertyContainerBuilderParams
     * @name m_NumberCount [type:int32_t] Number of items of type float
     * @name m_HashCount [type:int32_t] Number of items of type dmhash_t
     * @name m_URLStringCount [type:int32_t] Number of items of type const char*
     * @name m_URLStringSize [type:int32_t] Size of all url strings combined, including null terminators
     * @name m_URLCount [type:int32_t] Number of items of type dmMessage::URL
     * @name m_Vector3Count [type:int32_t] Number of items of type vector3 (float[3])
     * @name m_Vector4Count [type:int32_t] Number of items of type vector4 (float[4])
     * @name m_QuatCount [type:int32_t] Number of items of type quaternion (float[4])
     * @name m_BoolCount [type:int32_t] Number of items of type bool
     */
    struct PropertyContainerBuilderParams
    {
        PropertyContainerBuilderParams();
        uint32_t m_NumberCount;
        uint32_t m_HashCount;
        uint32_t m_URLStringCount;
        uint32_t m_URLStringSize;
        uint32_t m_URLCount;
        uint32_t m_Vector3Count;
        uint32_t m_Vector4Count;
        uint32_t m_QuatCount;
        uint32_t m_BoolCount;
    };

    /*#
     * Create a property container builder with memory preallocated
     * @name PropertyContainerCreateBuilder
     * @param params [type: PropertyContainerBuilderParams] The params holding the memory requirements
     * @return container [type: HPropertyContainerBuilder] The builder
     */
    HPropertyContainerBuilder PropertyContainerCreateBuilder(const PropertyContainerBuilderParams& params);

    /*#
     * Add a property of type float to the container
     * @name PropertyContainerPushFloat
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param value [type: float] The value of the property
     */
    void PropertyContainerPushFloat(HPropertyContainerBuilder builder, dmhash_t id, float value);

    /*#
     * Add a property of type float3 to the container
     * @name PropertyContainerPushVector3
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param values [type: float*] The value of the property (3 floats)
     */
    void PropertyContainerPushVector3(HPropertyContainerBuilder builder, dmhash_t id, const float* values);

    /*#
     * Add a property of type float4 to the container
     * @name PropertyContainerPushVector4
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param values [type: float*] The value of the property (4 floats)
     */
    void PropertyContainerPushVector4(HPropertyContainerBuilder builder, dmhash_t id, const float* values);

    /*#
     * Add a property of type float4 to the container
     * @name PropertyContainerPushQuat
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param values [type: float*] The value of the property (4 floats)
     */
    void PropertyContainerPushQuat(HPropertyContainerBuilder builder, dmhash_t id, const float* values);

    /*#
     * Add a property of type bool to the container
     * @name PropertyContainerPushBool
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param value [type: bool] The value of the property
     */
    void PropertyContainerPushBool(HPropertyContainerBuilder builder, dmhash_t id, bool value);

    /*#
     * Add a property of type dmhash_t to the container
     * @name PropertyContainerPushHash
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param value [type: dmhash_t] The value of the property
     */
    void PropertyContainerPushHash(HPropertyContainerBuilder builder, dmhash_t id, dmhash_t value);

    /*#
     * Add a property of type (url) string to the container
     * @name PropertyContainerPushURLString
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param value [type: const char*] The value of the property
     */
    void PropertyContainerPushURLString(HPropertyContainerBuilder builder, dmhash_t id, const char* value);

    /*#
     * Add a property of type dmMessage::URL to the container
     * @name PropertyContainerPushURL
     * @param builder [type: HPropertyContainerBuilder] The container builder
     * @param id [type: dmhash_t] The id of the property
     * @param value [type: dmMessage::URL] The value of the property
     */
    void PropertyContainerPushURL(HPropertyContainerBuilder builder, dmhash_t id, const char value[sizeof(dmMessage::URL)]);

    /*#
     * Creates the final property container
     * @name PropertyContainerCreate
     * @param builder [type: HPropertyContainerBuilder] The property container builder
     * @return container [type: HPropertyContainer] The property container
     */
    HPropertyContainer PropertyContainerCreate(HPropertyContainerBuilder builder);

    /*#
     * Deallocates a property container
     * @name PropertyContainerDestroy
     * @param container [type: HPropertyContainer] The property container
     */
    void PropertyContainerDestroy(HPropertyContainer container);

    /*#
     * Allocates and copies a property container
     * @name PropertyContainerCopy
     * @param original [type: HPropertyContainer] The original property container
     * @return container [type: HPropertyContainer] The new property container
     */
    HPropertyContainer PropertyContainerCopy(HPropertyContainer original);

    /*#
     * Merges two containers into a newly allocated container
     * The properties in the `overrides` container will take presedence.
     *
     * @name PropertyContainerMerge
     * @param original [type: HPropertyContainer] The original property container
     * @param overrides [type: HPropertyContainer] The container with override properties
     * @return container [type: HPropertyContainer] The merged property container
     */
    HPropertyContainer PropertyContainerMerge(HPropertyContainer original, HPropertyContainer overrides);

} // namespace dmGameObject

#endif // DMSDK_GAMEOBJECT_PROPS_H
