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

#ifndef DM_GAMESYS_COLLECTION_FACTORY_H
#define DM_GAMESYS_COLLECTION_FACTORY_H

#include <gameobject/component.h>
#include <resource/resource.h>

namespace dmGameSystem
{
    struct CollectionFactoryComponent;  // for scripting
    struct CollectionFactoryWorld;      // for scripting
    struct CollectionFactoryResource;   // for scripting

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::PropertyResult CompCollectionFactoryGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    // For scripting

    bool CompCollectionFactoryLoad(CollectionFactoryWorld* world, CollectionFactoryComponent* component, int callback_ref, int self_ref, int url_ref);
    bool CompCollectionFactoryUnload(CollectionFactoryWorld* world, CollectionFactoryComponent* component);

    /**
     * CompCollectionFactoryStatus
     */
    enum CompCollectionFactoryStatus
    {
        COMP_COLLECTION_FACTORY_STATUS_UNLOADED = 0,//!< COMP_COLLECTION_FACTORY_STATUS_UNLOADED
        COMP_COLLECTION_FACTORY_STATUS_LOADING = 1, //!< COMP_COLLECTION_FACTORY_STATUS_LOADING
        COMP_COLLECTION_FACTORY_STATUS_LOADED = 2,  //!< COMP_COLLECTION_FACTORY_STATUS_LOADED
    };

    dmResource::HFactory        CompCollectionFactoryGetResourceFactory(CollectionFactoryWorld* world);
    CompCollectionFactoryStatus CompCollectionFactoryGetStatus(CollectionFactoryComponent* component);
    bool                        CompCollectionFactoryIsDynamicPrototype(CollectionFactoryComponent* component);
    bool                        CompCollectionFactoryIsLoading(CollectionFactoryComponent* component);
    CollectionFactoryResource*  CompCollectionFactoryGetResource(CollectionFactoryComponent* component);
    CollectionFactoryResource*  CompCollectionFactoryGetDefaultResource(CollectionFactoryComponent* component);
    CollectionFactoryResource*  CompCollectionFactoryGetCustomResource(CollectionFactoryComponent* component);
    void                        CompCollectionFactorySetResource(CollectionFactoryComponent* component, CollectionFactoryResource* resource);
}

#endif
