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

#ifndef DMSDK_GAMESYS_RES_COLLECTION_FACTORY_H
#define DMSDK_GAMESYS_RES_COLLECTION_FACTORY_H

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/gameobject/gameobject.h>
#include <stdint.h>

namespace dmGameSystem
{
    struct CollectionFactoryResource
    {
        CollectionFactoryResource& operator=(CollectionFactoryResource& other);

        dmhash_t                        m_PrototypePathHash;
        dmGameObject::HCollectionDesc   m_CollectionDesc;
        dmArray<void*>                  m_CollectionResources;
        uint8_t                         m_LoadDynamically : 1;
        uint8_t                         m_DynamicPrototype : 1;
        uint8_t                         : 6;
    };

    // for scripting
    dmResource::Result  ResCollectionFactoryLoadResource(dmResource::HFactory factory, const char* collectionc, bool load_dynamically, bool dynamic_prototype,
                                                            CollectionFactoryResource** out_res);
    void                ResCollectionFactoryDestroyResource(dmResource::HFactory factory, CollectionFactoryResource* resource);
}

#endif // DMSDK_GAMESYS_RES_FACTORY_H
