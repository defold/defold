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

#ifndef DMSDK_GAMESYS_COLLECTION_PROXY_H
#define DMSDK_GAMESYS_COLLECTION_PROXY_H

#include <dmsdk/gameobject/gameobject.h>

namespace dmGameSystem
{
    typedef struct CollectionProxyWorld* HCollectionProxyWorld;
    typedef struct CollectionProxyComponent* HCollectionProxyComponent;

    typedef void (*ProxyLoadCallback)(const char* path, dmGameObject::Result result, void* user_ctx);

    dmGameObject::Result CompCollectionProxyLoad(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx);
    dmGameObject::Result CompCollectionProxyLoadAsync(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx);

    dmGameObject::Result CompCollectionProxyUnload(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx);

    // The same effect as sending the "init" message to the proxy
    dmGameObject::Result CompCollectionProxyInitialize(HCollectionProxyWorld world, HCollectionProxyComponent proxy);
    // The same effect as sending the "finalize" message to the proxy
    dmGameObject::Result CompCollectionProxyFinalize(HCollectionProxyWorld world, HCollectionProxyComponent proxy);
    // The same effect as sending the "enable" message to the proxy
    dmGameObject::Result CompCollectionProxyEnable(HCollectionProxyWorld world, HCollectionProxyComponent proxy);
    // The same effect as sending the "disable" message to the proxy
    dmGameObject::Result CompCollectionProxyDisable(HCollectionProxyWorld world, HCollectionProxyComponent proxy);

    // The same effect as sending the "set_time_step" message to the proxy
    // factor: multiplier (default = 1.0)
    // Mode: 0 = continuous, 1 = discrete (default = 0)
    dmGameObject::Result CompCollectionProxySetTimeStep(HCollectionProxyWorld world, HCollectionProxyComponent proxy, float factor, int mode);
}

#endif // DMSDK_GAMESYS_COLLECTION_PROXY_H
