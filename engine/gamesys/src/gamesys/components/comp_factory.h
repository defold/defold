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

#ifndef DM_GAMESYS_FACTORY_H
#define DM_GAMESYS_FACTORY_H

#include <gameobject/component.h>
#include <dmsdk/gamesys/components/comp_factory.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult      CompFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    dmGameObject::CreateResult      CompFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    dmGameObject::CreateResult      CompFactoryCreate(const dmGameObject::ComponentCreateParams& params);
    dmGameObject::CreateResult      CompFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);
    dmGameObject::CreateResult      CompFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    dmGameObject::UpdateResult      CompFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult      CompFactoryOnMessage(const dmGameObject::ComponentOnMessageParams& params);
    dmGameObject::PropertyResult    CompFactoryGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);
    void*                           CompFactoryGetComponent(const dmGameObject::ComponentGetParams& params);

    dmResource::HFactory CompFactoryGetResourceFactory(HFactoryWorld world);
    dmGameObject::HPrototype CompFactoryGetPrototype(HFactoryWorld world, HFactoryComponent component);
    const char*         CompFactoryGetPrototypePath(HFactoryWorld world, HFactoryComponent component);

    bool                CompFactoryLoad(HFactoryWorld world, HFactoryComponent component, int callback_ref, int self_ref, int url_ref);
    bool                CompFactoryUnload(HFactoryWorld world, HFactoryComponent component);

    void                CompFactorySetResource(HFactoryWorld world, HFactoryComponent component, HFactoryResource resource);
    HFactoryResource    CompFactoryGetResource(HFactoryWorld world, HFactoryComponent component);
    HFactoryResource    CompFactoryGetDefaultResource(HFactoryWorld world, HFactoryComponent component);
    HFactoryResource    CompFactoryGetCustomResource(HFactoryWorld world, HFactoryComponent component);

}

#endif
