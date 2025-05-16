// Copyright 2020-2025 The Defold Foundation
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
#include <dmsdk/dlib/hashtable.h>

namespace dmGameObject
{
    typedef struct CollectionHandle* HCollection;
    typedef dmHashTable<dmhash_t, HPropertyContainer> InstancePropertyBuffers;
    typedef dmHashTable<dmhash_t, dmhash_t> InstanceIdMap;
}

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

    bool CompCollectionFactorySpawn(HCollectionFactoryComponent component, dmGameObject::HCollection collection,
                                    const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                    dmGameObject::InstancePropertyBuffers* properties, dmGameObject::InstanceIdMap* out_instances);
}

#endif // DMSDK_GAMESYS_COLLECTION_FACTORY_H
