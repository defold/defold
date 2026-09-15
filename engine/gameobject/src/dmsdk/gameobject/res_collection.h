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

#ifndef DMSDK_GAMEOBJECT_RES_COLLECTION_H
#define DMSDK_GAMEOBJECT_RES_COLLECTION_H

#include <dmsdk/gameobject/gameobject.h>

/*# Collection resource functions
 *
 * API for accessing loaded game object collection resources.
 *
 * @document
 * @name Collection Resource
 * @namespace dmGameObject
 * @language C++
 */

namespace dmGameObject
{
    /*# collection resource
     * Opaque pointer returned by dmResource::Get for a compiled collection
     * resource. The pointer remains valid while the caller holds a resource
     * reference, including across resource reloads.
     * @struct
     * @name CollectionResource
     */
    struct CollectionResource;

    /*# get a collection handle from a collection resource
     * Converts a live collection resource returned by dmResource::Get into the
     * numeric collection handle used by the game-object API. The caller retains
     * ownership of the resource reference and must release the original resource
     * pointer with dmResource::Release. The returned handle may change when the
     * resource is reloaded; call this function again after a reload.
     * @name GetCollectionFromResource
     * @param resource [type: dmGameObject::CollectionResource*] Live collection resource returned by dmResource::Get.
     * @return collection [type: dmGameObject::HCollection] Collection handle, or dmGameObject::INVALID_COLLECTION if resource is null.
     */
    HCollection GetCollectionFromResource(CollectionResource* resource);
}

#endif // DMSDK_GAMEOBJECT_RES_COLLECTION_H
