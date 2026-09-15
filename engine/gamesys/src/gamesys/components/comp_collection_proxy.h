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

#ifndef DM_GAMESYS_COLLECTION_PROXY_H
#define DM_GAMESYS_COLLECTION_PROXY_H

#include <gameobject/component.h>
#include <dmsdk/gamesys/components/comp_collection_proxy.h>

namespace dmGameSystem
{

    /*# Result of changing collection in a collection proxy
     *
     * Result of changing collection in a collection proxy
     *
     * @enum
     * @name SetCollectionForProxyPathResult
     * @member dmGameSystem::SET_COLLECTION_PATH_RESULT_OK
     * @member dmGameSystem::SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING
     * @member dmGameSystem::SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED
     * @member dmGameSystem::SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED
     */
    enum SetCollectionForProxyPathResult
    {
        SET_COLLECTION_PATH_RESULT_OK = 0,
        SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING = -1,
        SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED = -2,
        SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED = -3
    };

    dmhash_t GetCollectionUrlHashFromComponent(const HCollectionProxyWorld world, dmhash_t instanceId, uint32_t index);

    SetCollectionForProxyPathResult CollectionProxySetCollectionPath(const HCollectionProxyWorld world, HCollectionProxyComponent component, const char* path);

    dmGameObject::CreateResult CompCollectionProxyNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionProxyDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionProxyFinal(const dmGameObject::ComponentFinalParams& params);

    dmGameObject::CreateResult CompCollectionProxyAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCollectionProxyRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::InputResult CompCollectionProxyOnInput(const dmGameObject::ComponentOnInputParams& params);

    void CompCollectionProxyIterChildren(dmGameObject::SceneNodeIterator* it, dmGameObject::SceneNode* node);
}

#endif // DM_GAMESYS_COLLECTION_PROXY_H
