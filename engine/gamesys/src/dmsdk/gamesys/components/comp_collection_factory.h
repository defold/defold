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

#ifndef DMSDK_GAMESYS_COLLECTION_FACTORY_H
#define DMSDK_GAMESYS_COLLECTION_FACTORY_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gameobject/gameobject.h>

/*# Collection factory component functions
 *
 * API for spawning collections from a collection factory component.
 *
 * @document
 * @name Collection factory
 * @namespace dmGameSystem
 * @language C++
 */

namespace dmGameSystem
{
    typedef struct CollectionFactoryWorld* HCollectionFactoryWorld;
    typedef struct CollectionFactoryComponent* HCollectionFactoryComponent;
    typedef struct CollectionFactoryResource* HCollectionFactoryResource;

    /*#
     * CompCollectionFactoryStatus
     * @enum
     */
    enum CompCollectionFactoryStatus
    {
        COMP_COLLECTION_FACTORY_STATUS_UNLOADED = 0,//!< COMP_COLLECTION_FACTORY_STATUS_UNLOADED
        COMP_COLLECTION_FACTORY_STATUS_LOADING = 1, //!< COMP_COLLECTION_FACTORY_STATUS_LOADING
        COMP_COLLECTION_FACTORY_STATUS_LOADED = 2,  //!< COMP_COLLECTION_FACTORY_STATUS_LOADED
    };

    CompCollectionFactoryStatus CompCollectionFactoryGetStatus(CollectionFactoryComponent* component);
    bool                        CompCollectionFactoryIsLoading(CollectionFactoryComponent* component);
    bool                        CompCollectionFactoryIsDynamicPrototype(CollectionFactoryComponent* component);

    /*# 
     * Spawns a collection of gameobjects in a collection using a collection factory component.
     * @name CompCollectionFactorySpawn
     * @param world [type: HCollectionFactoryWorld] Collection factory world
     * @param component [type: HCollectionFactoryComponent] Collection factory component
     * @param collection [type: HCollection] Gameobject collection to spawn into
     * @param id_prefix [type: const char*] Prefix for the spawned instance identifiers. Must start with a forward slash (/). Must be unique within the collection. Pass nullptr to automatically generate a unique identifier prefix (e.g. /collection1, /collection2 etc.).
     * @param position [type: dmVMath::Point3] Position of the spawned objects
     * @param rotation [type: dmVMath::Quat] Rotation of the spawned objects
     * @param scale [type: dmVMath::Vector3] Scale of the spawned objects
     * @param properties [type: dmGameObject::InstancePropertyContainers] Property containers with override properties
     * @param out_instances [type: dmGameObject::InstanceIdMap] A map with the spawned instance id's
     * @return result [type: dmGameObject::Result] Result of the operation
     */
    dmGameObject::Result CompCollectionFactorySpawn(HCollectionFactoryWorld world, HCollectionFactoryComponent component, dmGameObject::HCollection collection,
                                    const char* id_prefix, 
                                    const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                    dmGameObject::InstancePropertyContainers* properties, dmGameObject::InstanceIdMap* out_instances);
}

#endif // DMSDK_GAMESYS_COLLECTION_FACTORY_H
