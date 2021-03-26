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

#ifndef DM_GAMESYS_COLLECTION_FACTORY_H
#define DM_GAMESYS_COLLECTION_FACTORY_H

#include <gameobject/component.h>

namespace dmGameSystem
{
    struct CollectionFactoryResource;

    // Visible due to scripting :(
    struct CollectionFactoryComponent
    {
        void Init();

        CollectionFactoryResource*  m_Resource;

        dmResource::HPreloader      m_Preloader;
        int m_PreloaderCallbackRef;
        int m_PreloaderSelfRef;
        int m_PreloaderURLRef;
        uint32_t m_Loading : 1;

        uint32_t m_AddedToUpdate : 1;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    // For scripting


    bool CompCollectionFactoryLoad(dmGameObject::HCollection collection, CollectionFactoryComponent* component);

    bool CompCollectionFactoryUnload(dmGameObject::HCollection collection, CollectionFactoryComponent* component);

    /**
     * CompCollectionFactoryStatus
     */
    enum CompCollectionFactoryStatus
    {
        COMP_COLLECTION_FACTORY_STATUS_UNLOADED = 0,//!< COMP_COLLECTION_FACTORY_STATUS_UNLOADED
        COMP_COLLECTION_FACTORY_STATUS_LOADING = 1, //!< COMP_COLLECTION_FACTORY_STATUS_LOADING
        COMP_COLLECTION_FACTORY_STATUS_LOADED = 2,  //!< COMP_COLLECTION_FACTORY_STATUS_LOADED
    };


    CompCollectionFactoryStatus CompCollectionFactoryGetStatus(CollectionFactoryComponent* component);
}

#endif
