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

#ifndef DM_GAMESYS_FACTORY_H
#define DM_GAMESYS_FACTORY_H

#include <gameobject/component.h>

namespace dmGameSystem
{
    struct FactoryWorld;
    struct FactoryComponent;

    dmGameObject::CreateResult CompFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    dmGameObject::CreateResult CompFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    dmGameObject::CreateResult CompFactoryCreate(const dmGameObject::ComponentCreateParams& params);
    dmGameObject::CreateResult CompFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);
    dmGameObject::CreateResult CompFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    dmGameObject::UpdateResult CompFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult CompFactoryOnMessage(const dmGameObject::ComponentOnMessageParams& params);
    dmGameObject::PropertyResult CompFactoryGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    // For scripting
    struct FactoryResource;

    /**
     * CompFactoryStatus
     */
    enum CompFactoryStatus
    {
        COMP_FACTORY_STATUS_UNLOADED = 0,//!< COMP_FACTORY_STATUS_UNLOADED
        COMP_FACTORY_STATUS_LOADING = 1, //!< COMP_FACTORY_STATUS_LOADING
        COMP_FACTORY_STATUS_LOADED = 2,  //!< COMP_FACTORY_STATUS_LOADED
    };

    dmResource::HFactory CompFactoryGetResourceFactory(FactoryWorld* world);

    dmGameObject::HPrototype CompFactoryGetPrototype(dmGameObject::HCollection collection, FactoryComponent* component);
    const char*         CompFactoryGetPrototypePath(FactoryComponent* component);
    bool                CompFactoryLoad(dmGameObject::HCollection collection, FactoryComponent* component, int callback_ref, int self_ref, int url_ref);
    bool                CompFactoryUnload(dmGameObject::HCollection collection, FactoryComponent* component);
    CompFactoryStatus   CompFactoryGetStatus(FactoryComponent* component);
    bool                CompFactoryIsLoading(FactoryComponent* component);
    bool                CompFactoryIsDynamicPrototype(FactoryComponent* component);
    void                CompFactorySetResource(FactoryComponent* component, FactoryResource* resource);
    FactoryResource*    CompFactoryGetResource(FactoryComponent* component);
    FactoryResource*    CompFactoryGetDefaultResource(FactoryComponent* component);
    FactoryResource*    CompFactoryGetCustomResource(FactoryComponent* component);
}

#endif
