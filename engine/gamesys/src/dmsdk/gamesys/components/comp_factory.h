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

#ifndef DMSDK_GAMESYS_FACTORY_H
#define DMSDK_GAMESYS_FACTORY_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>

namespace dmGameObject
{
    typedef struct CollectionHandle* HCollection;
    typedef struct PropertyContainer* HPropertyContainer;
    typedef struct Instance* HInstance;
}

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

    dmGameObject::HInstance CompFactorySpawn(HFactoryWorld world, HFactoryComponent component, dmGameObject::HCollection collection,
                                                uint32_t index, dmhash_t id,
                                                const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                                dmGameObject::HPropertyContainer properties);
}

#endif // DMSDK_GAMESYS_FACTORY_H
