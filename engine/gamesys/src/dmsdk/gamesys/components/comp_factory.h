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

#ifndef DMSDK_GAMESYS_FACTORY_H
#define DMSDK_GAMESYS_FACTORY_H

/*# Factory component functions
 *
 * API for spawning gameobject instances from a factory component.
 *
 * @document
 * @name Factory
 * @namespace dmGameSystem
 * @language C++
 */

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gameobject/gameobject.h>

namespace dmGameSystem
{
    typedef struct FactoryWorld* HFactoryWorld;
    typedef struct FactoryComponent* HFactoryComponent;
    typedef struct FactoryResource* HFactoryResource;

    /*#
     * CompFactoryStatus
     * @enum
     */
    enum CompFactoryStatus
    {
        COMP_FACTORY_STATUS_UNLOADED = 0,//!< COMP_FACTORY_STATUS_UNLOADED
        COMP_FACTORY_STATUS_LOADING = 1, //!< COMP_FACTORY_STATUS_LOADING
        COMP_FACTORY_STATUS_LOADED = 2,  //!< COMP_FACTORY_STATUS_LOADED
    };

    CompFactoryStatus   CompFactoryGetStatus(HFactoryWorld world, HFactoryComponent component);
    bool                CompFactoryIsLoading(HFactoryWorld world, HFactoryComponent component);
    bool                CompFactoryIsDynamicPrototype(HFactoryWorld world, HFactoryComponent component);

    /*# 
     * Spawns a new gameobject instance in a collection using a factory component.
     * @name CompFactorySpawn
     * @param world [type: HFactoryWorld] Factory world
     * @param component [type: HFactoryComponent] Factory component
     * @param collection [type: HCollection] Gameobject collection to spawn into
     * @param id [type: dmhash_t] Identifier for the new instance. Must be unique within the collection. Pass 0 to automatically generate a unique identifier (e.g. /instance1, /instance2 etc.).
     * @param position [type: dmVMath::Point3] Position of the spawned object
     * @param rotation [type: dmVMath::Quat] Rotation of the spawned object
     * @param scale [type: dmVMath::Vector3] Scale of the spawned object
     * @param properties [type: dmGameObject::HPropertyContainer] Property container with override properties
     * @param out_instance [type: dmGameObject::HInstance] Output parameter for the new instance
     * @return result [type: dmGameObject::Result] Result of the operation
     */
    dmGameObject::Result CompFactorySpawn(HFactoryWorld world, HFactoryComponent component, dmGameObject::HCollection collection, dmhash_t id,
                                                const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                                dmGameObject::HPropertyContainer properties, dmGameObject::HInstance* out_instance);
}

#endif // DMSDK_GAMESYS_FACTORY_H
