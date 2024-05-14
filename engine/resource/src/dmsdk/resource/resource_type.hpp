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

#ifndef DMSDK_RESOURCE_TYPE_HPP
#define DMSDK_RESOURCE_TYPE_HPP

/*# Resource type
 *
 * Functions for managing resource types.
 *
 * @document
 * @name ResourceType
 * @path engine/resource/src/dmsdk/resource/resource_type.hpp
 */

#include <stdint.h>

#include <dmsdk/resource/resource.hpp>
#include <dmsdk/resource/resource_params.hpp>
#include <dmsdk/resource/resource_desc.hpp>

#include <dmsdk/resource/resource_type_gen.hpp>

namespace dmResource {
    static const uint32_t s_ResourceTypeCreatorDescBufferSize = ResourceTypeCreatorDescBufferSize;

    /*#
     * @note Deprecated in favor of ResourceTypeSetPreloadFn
     */
    typedef dmResource::Result (*FResourcePreload)(const dmResource::ResourcePreloadParams* params);

    /*#
     * @note Deprecated in favor of ResourceTypeSetCreateFn
     */
    typedef dmResource::Result (*FResourceCreate)(const dmResource::ResourceCreateParams* params);

    /*#
     * @note Deprecated in favor of ResourceTypeSetPostCreateFn
     */
    typedef dmResource::Result (*FResourcePostCreate)(const dmResource::ResourcePostCreateParams* params);

    /*#
     * @note Deprecated in favor of ResourceTypeSetDestroyFn
     */
    typedef dmResource::Result (*FResourceDestroy)(const dmResource::ResourceDestroyParams* params);

    /*#
     * @note Deprecated in favor of ResourceTypeSetRecreateFn
     */
    typedef dmResource::Result (*FResourceRecreate)(const dmResource::ResourceRecreateParams* params);


    // void RegisterTypeCreatorDesc(void* desc,
    //                              uint32_t size,
    //                              const char *name,
    //                              FResourceTypeRegister register_fn,
    //                              FResourceTypeDeregister deregister_fn);

    /*#
     * @note Deprecated in favor of ResourceRegisterTypeCreatorDesc
     * @name RegisterType
     */
    Result RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           dmResource::FResourcePreload preload_function,
                           dmResource::FResourceCreate create_function,
                           dmResource::FResourcePostCreate post_create_function,
                           dmResource::FResourceDestroy destroy_function,
                           dmResource::FResourceRecreate recreate_function);

    /*#
     * Setup function pointers and context for a resource type
     * @note C++ Helper function. Deprecated in favor of ResourceRegisterTypeCreatorDesc et al
     * @name SetupType
     */
    Result SetupType(HResourceTypeContext ctx,
                     HResourceType type,
                     void* context,
                     FResourcePreload preload_function,
                     FResourceCreate create_function,
                     FResourcePostCreate post_create_function,
                     FResourceDestroy destroy_function,
                     FResourceRecreate recreate_function);

    // Internal (do not use!)
    HResourceType AllocateResourceType(HFactory factory, const char* extension);
    void          FreeResourceType(HFactory factory, HResourceType type);
    Result        ValidateResourceType(HResourceType type);
}


#endif // DMSDK_RESOURCE_TYPE_HPP
