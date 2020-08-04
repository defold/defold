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

#ifndef DM_GAMESYS_RES_COLLECTION_FACTORY_H
#define DM_GAMESYS_RES_COLLECTION_FACTORY_H

#include <stdint.h>

#include <dlib/array.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    struct CollectionFactoryResource
    {
        CollectionFactoryResource& operator=(CollectionFactoryResource& other)
        {
            m_CollectionDesc = other.m_CollectionDesc;
            m_CollectionResources.Swap(other.m_CollectionResources);
            m_LoadDynamically = other.m_LoadDynamically;
            return *this;
        }

        dmGameObject::HCollectionDesc m_CollectionDesc;
        dmArray<void*> m_CollectionResources;
        bool m_LoadDynamically;
    };

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_FACTORY_H
