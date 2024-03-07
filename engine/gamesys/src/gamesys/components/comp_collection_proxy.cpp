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

#include "comp_collection_proxy.h"
#include "resources/res_collection_proxy.h"

#include <string.h>

#include <dmsdk/dlib/vmath.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/profile.h>

#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>

#include <resource/resource.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include <gamesys/gamesys_ddf.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_CollectionProxy, 0, FrameReset, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_CollectionProxyLoaded, 0, FrameReset, "# loaded collection proxies", &rmtp_CollectionProxy);
DM_PROPERTY_U32(rmtp_CollectionProxyEnabled, 0, FrameReset, "# enabled collection proxies", &rmtp_CollectionProxy);

namespace dmGameSystem
{
    /*# Collection proxy API documentation
     *
     * Messages for controlling and interacting with collection proxies
     * which are used to dynamically load collections into the runtime.
     *
     * @document
     * @name Collection proxy
     * @namespace collectionproxy
     */

    using namespace dmVMath;

    const char* COLLECTION_PROXY_MAX_COUNT_KEY = "collection_proxy.max_count";

    static const dmhash_t COLLECTION_PROXY_LOAD_HASH = dmHashString64("load");
    static const dmhash_t COLLECTION_PROXY_ASYNC_LOAD_HASH = dmHashString64("async_load");
    static const dmhash_t COLLECTION_PROXY_UNLOAD_HASH = dmHashString64("unload");
    static const dmhash_t COLLECTION_PROXY_INIT_HASH = dmHashString64("init");
    static const dmhash_t COLLECTION_PROXY_FINAL_HASH = dmHashString64("final");
    static const dmhash_t COLLECTION_PROXY_LOADED_HASH = dmHashString64("proxy_loaded");
    static const dmhash_t COLLECTION_PROXY_UNLOADED_HASH = dmHashString64("proxy_unloaded");

    struct CollectionProxyComponent
    {
        dmMessage::URL                  m_Unloader;
        CollectionProxyResource*        m_Resource;
        dmGameObject::HCollection       m_Collection;
        dmGameObject::HInstance         m_Instance;
        dmGameSystemDDF::TimeStepMode   m_TimeStepMode;
        float                           m_TimeStepFactor;
        float                           m_AccumulatedTime;
        uint32_t                        m_ComponentIndex : 16;
        uint32_t                        m_Initialized : 1;
        uint32_t                        m_Enabled : 1;
        uint32_t                        m_DelayedEnable : 1;
        uint32_t                        m_Unloaded : 1;
        uint32_t                        m_AddedToUpdate : 1;

        dmResource::HPreloader          m_Preloader;
        dmMessage::URL                  m_LoadSender, m_LoadReceiver;

        ProxyLoadCallback               m_Callback;
        void*                           m_CallbackCtx;
    };

    struct CollectionProxyWorld
    {
        dmArray<CollectionProxyComponent>   m_Components;
        dmIndexPool32                       m_IndexPool;
        CollectionProxyContext*             m_Context;
    };

    static dmGameObject::UpdateResult DoLoad(dmResource::HFactory factory, CollectionProxyComponent* proxy)
    {
        dmResource::Result result = dmResource::Get(factory, proxy->m_Resource->m_DDF->m_Collection, (void**)&proxy->m_Collection);
        if (result != dmResource::RESULT_OK)
        {
            dmLogError("The collection %s could not be loaded.", proxy->m_Resource->m_DDF->m_Collection);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void LoadComplete(CollectionProxyComponent* proxy, dmGameObject::Result result)
    {
        if (proxy->m_Callback)
        {
            proxy->m_Callback(proxy->m_Resource->m_DDF->m_Collection, result, proxy->m_CallbackCtx);
        }
        else if(dmGameObject::RESULT_OK == result)
        {
            // We only post a "proxy_loaded" if the loading went ok
            if (dmMessage::IsSocketValid(proxy->m_LoadSender.m_Socket))
            {
                dmMessage::Result msg_result = dmMessage::Post(&proxy->m_LoadReceiver, &proxy->m_LoadSender, COLLECTION_PROXY_LOADED_HASH, 0, 0, 0, 0, 0);
                if (msg_result != dmMessage::RESULT_OK)
                {
                    dmLogWarning("proxy_loaded could not be posted: %d", msg_result);
                }
            }
        }
    }

    void UnloadComplete(CollectionProxyComponent* proxy, dmGameObject::Result result)
    {
        proxy->m_Unloaded = 0;
        if (proxy->m_Callback)
        {
            proxy->m_Callback(proxy->m_Resource->m_DDF->m_Collection, result, proxy->m_CallbackCtx);
        }
        else if (dmMessage::IsSocketValid(proxy->m_Unloader.m_Socket))
        {
            dmMessage::URL sender;
            sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(proxy->m_Instance));
            sender.m_Path = dmGameObject::GetIdentifier(proxy->m_Instance);
            dmGameObject::GetComponentId(proxy->m_Instance, proxy->m_ComponentIndex, &sender.m_Fragment);
            dmMessage::Result msg_result = dmMessage::Post(&sender, &proxy->m_Unloader, COLLECTION_PROXY_UNLOADED_HASH, 0, 0, 0, 0, 0);
            if (msg_result != dmMessage::RESULT_OK)
            {
                dmLogWarning("proxy_unloaded could not be posted: %d", msg_result);
            }
        }
    }

    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        return (DoLoad(params->m_Factory, (CollectionProxyComponent *) params->m_UserData) == dmGameObject::UPDATE_RESULT_OK);
    }


    dmhash_t GetUrlHashFromComponent(const HCollectionProxyWorld world, dmhash_t instanceId, uint32_t index)
    {
        dmhash_t comp_url_hash = 0;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            dmGameSystem::CollectionProxyComponent* c = &world->m_Components[i];

            if (c == 0x0)
            {
                continue;
            }

            dmhash_t component_instance_id = dmGameObject::GetIdentifier(c->m_Instance);
            if (component_instance_id == instanceId && c->m_ComponentIndex == index)
            {
                comp_url_hash = c->m_Resource->m_UrlHash;
                break;
            }
        }

        return comp_url_hash;
    }

    dmGameObject::CreateResult CompCollectionProxyNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CollectionProxyWorld* proxy_world = new CollectionProxyWorld();
        CollectionProxyContext* context = (CollectionProxyContext*)params.m_Context;
        proxy_world->m_Context = context;
        uint32_t component_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxCollectionProxyCount);
        proxy_world->m_Components.SetCapacity(component_count);
        proxy_world->m_Components.SetSize(component_count);
        memset(proxy_world->m_Components.Begin(), 0, sizeof(CollectionProxyComponent) * component_count);
        proxy_world->m_IndexPool.SetCapacity(component_count);
        *params.m_World = proxy_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        CollectionProxyContext* context = (CollectionProxyContext*)params.m_Context;
        dmResource::HFactory factory = context->m_Factory;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            dmGameObject::HCollection collection = proxy_world->m_Components[i].m_Collection;
            if (collection != 0)
            {
                if (proxy_world->m_Components[i].m_Initialized)
                    dmGameObject::Final(collection);
                dmResource::Release(factory, collection);
            }
        }
        delete proxy_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        if (proxy_world->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = proxy_world->m_IndexPool.Pop();
            CollectionProxyComponent* proxy = &proxy_world->m_Components[index];
            memset(proxy, 0, sizeof(CollectionProxyComponent));
            proxy->m_TimeStepFactor = 1.0f;
            proxy->m_Resource = (CollectionProxyResource*) params.m_Resource;
            proxy->m_Instance = params.m_Instance;
            proxy->m_ComponentIndex = params.m_ComponentIndex;
            *params.m_UserData = (uintptr_t) proxy;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            ShowFullBufferError("Collection proxy", COLLECTION_PROXY_MAX_COUNT_KEY, proxy_world->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCollectionProxyDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*)*params.m_UserData;
        CollectionProxyContext* context = (CollectionProxyContext*)params.m_Context;
        if(proxy->m_Preloader)
        {
            dmResource::DeletePreloader(proxy->m_Preloader);
        }
        if (proxy->m_Collection != 0)
        {
            dmResource::Release(context->m_Factory, proxy->m_Collection);
        }
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        uint32_t index = proxy - &proxy_world->m_Components[0];
        proxy_world->m_IndexPool.Push(index);
        memset(proxy, 0, sizeof(CollectionProxyComponent));
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyFinal(const dmGameObject::ComponentFinalParams& params)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*)*params.m_UserData;
        if (proxy->m_Initialized)
        {
            proxy->m_Initialized = 0;
            dmGameObject::Final(proxy->m_Collection);
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*)*params.m_UserData;
        proxy->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollectionProxyUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            CollectionProxyComponent* proxy = &proxy_world->m_Components[i];
            if (!proxy->m_AddedToUpdate) {
                continue;
            }
            DM_PROPERTY_ADD_U32(rmtp_CollectionProxy, 1);
            if (proxy->m_Preloader != 0)
            {
                CollectionProxyContext* context = (CollectionProxyContext*)params.m_Context;
                dmResource::PreloaderCompleteCallbackParams params;
                params.m_Factory = context->m_Factory;
                params.m_UserData = proxy;
                dmResource::Result r = dmResource::UpdatePreloader(proxy->m_Preloader, PreloadCompleteCallback, &params, 10*1000);
                if (r != dmResource::RESULT_PENDING)
                {
                    dmResource::DeletePreloader(proxy->m_Preloader);
                    LoadComplete(proxy, dmResource::RESULT_OK == r ? dmGameObject::RESULT_OK : dmGameObject::RESULT_UNKNOWN_ERROR);
                    proxy->m_Preloader = 0;
                }
            }
            if (proxy->m_Collection != 0)
            {
                DM_PROPERTY_ADD_U32(rmtp_CollectionProxyLoaded, 1);
                if (proxy->m_DelayedEnable != proxy->m_Enabled)
                {
                    proxy->m_Enabled = proxy->m_DelayedEnable;
                }

                if (proxy->m_Enabled)
                {
                    DM_PROPERTY_ADD_U32(rmtp_CollectionProxyEnabled, 1);
                    dmGameObject::UpdateContext uc = *params.m_UpdateContext;
                    // We might be inside a parent proxy, so the scale will propagate
                    uc.m_TimeScale = params.m_UpdateContext->m_TimeScale * proxy->m_TimeStepFactor;

                    float warped_dt = params.m_UpdateContext->m_DT * proxy->m_TimeStepFactor;
                    switch (proxy->m_TimeStepMode)
                    {
                    case dmGameSystemDDF::TIME_STEP_MODE_CONTINUOUS:
                        uc.m_DT = warped_dt;
                        proxy->m_AccumulatedTime = 0.0f;
                        break;
                    case dmGameSystemDDF::TIME_STEP_MODE_DISCRETE:
                        proxy->m_AccumulatedTime += warped_dt;
                        if (proxy->m_AccumulatedTime >= params.m_UpdateContext->m_DT)
                        {
                            uc.m_DT = params.m_UpdateContext->m_DT;
                            proxy->m_AccumulatedTime -= params.m_UpdateContext->m_DT;
                        }
                        else
                        {
                            uc.m_DT = 0.0f;
                        }
                        break;
                    default:
                        break;
                    }

                    if (!dmGameObject::Update(proxy->m_Collection, &uc))
                        result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                }
                else
                {
                    proxy->m_AccumulatedTime = 0.0f;
                }
            }
            if (proxy->m_Unloaded)
            {
                UnloadComplete(proxy, dmGameObject::RESULT_OK);
            }
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionProxyRender(const dmGameObject::ComponentsRenderParams& params)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            CollectionProxyComponent* proxy = &proxy_world->m_Components[i];
            if (proxy->m_Collection != 0 && proxy->m_Enabled)
            {
                if (!dmGameObject::Render(proxy->m_Collection))
                    result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            CollectionProxyComponent* proxy = &proxy_world->m_Components[i];
            if (proxy->m_Collection != 0)
            {
                if (proxy->m_Enabled)
                {
                    if (!dmGameObject::PostUpdate(proxy->m_Collection))
                        result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                }
            }
        }
        return result;
    }

    /*# sets the time-step for update
     *
     * Post this message to a collection-proxy-component to modify the time-step used when updating the collection controlled by the proxy.
     * The time-step is modified by a scaling `factor` and can be incremented either continuously or in discrete steps.
     *
     * The continuous mode can be used for slow-motion or fast-forward effects.
     *
     * The discrete mode is only useful when scaling the time-step to pass slower than real time (`factor` is below 1).
     * The time-step will then be set to 0 for as many frames as the scaling demands and then take on the full real-time-step for one frame,
     * to simulate pulses. E.g. if `factor` is set to `0.1` the time-step would be 0 for 9 frames, then be 1/60 for one
     * frame, 0 for 9 frames, and so on. The result in practice is that the game looks like it's updated at a much lower frequency than 60 Hz,
     * which can be useful for debugging when each frame needs to be inspected.
     *
     * @message
     * @name set_time_step
     * @param factor [type:number] time-step scaling factor
     * @param mode [type:number] time-step mode: 0 for continuous and 1 for discrete
     * @examples
     *
     * The examples assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * Update the collection twice as fast:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 2, mode = 0})
     * ```
     *
     * Update the collection twice as slow:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 0.5, mode = 0})
     * ```
     *
     * Simulate 1 FPS for the collection:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 1/60, mode = 1})
     * ```
     */


    static dmGameObject::Result CompCollectionProxyLoadInternal(CollectionProxyContext* context, HCollectionProxyComponent proxy,
                                                            ProxyLoadCallback cbk, void* cbk_ctx,
                                                            dmMessage::URL* sender, dmMessage::URL* receiver,
                                                            dmMessage::Message* message,
                                                            bool load_async)
    {
        assert(context != 0);
        assert(proxy != 0);

        if (proxy->m_Collection != 0)
        {
            LogMessageError(message, "The collection %s could not be loaded since it was already.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK;
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        const char* path = proxy->m_Resource->m_DDF->m_Collection;
        if (proxy->m_Collection != 0)
        {
            LogMessageError(message, "Collection proxy %s: '%s'", "already loaded", path);
            if (message)
                return dmGameObject::RESULT_OK;
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        if (proxy->m_Preloader != 0)
        {
            LogMessageError(message, "Collection proxy %s: '%s'", "already being loaded", path);
            if (message)
                return dmGameObject::RESULT_OK;
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        proxy->m_Unloaded = 0;
        if (sender)
            proxy->m_LoadSender = *sender;
        else
            dmMessage::ResetURL(&proxy->m_LoadSender);

        if (receiver)
            proxy->m_LoadReceiver = *receiver;
        else
            dmMessage::ResetURL(&proxy->m_LoadReceiver);

        proxy->m_Callback = cbk;
        proxy->m_CallbackCtx = cbk_ctx;

        if (load_async)
        {
            proxy->m_Preloader = dmResource::NewPreloader(context->m_Factory, path);
            return dmGameObject::RESULT_OK;
        }
        else
        {
            dmGameObject::UpdateResult res = DoLoad(context->m_Factory, proxy);
            bool loaded_ok = dmGameObject::UPDATE_RESULT_OK == res;
            LoadComplete(proxy, loaded_ok ? dmGameObject::RESULT_OK : dmGameObject::RESULT_UNKNOWN_ERROR);
            return loaded_ok ? dmGameObject::RESULT_OK : dmGameObject::RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::Result CompCollectionProxyLoadAsync(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx)
    {
        return CompCollectionProxyLoadInternal(world->m_Context, proxy, cbk, cbk_ctx, 0, 0, 0, true);
    }

    dmGameObject::Result CompCollectionProxyLoad(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx)
    {
        return CompCollectionProxyLoadInternal(world->m_Context, proxy, cbk, cbk_ctx, 0, 0, 0, false);
    }

    static dmGameObject::Result CompCollectionProxyUnloadInternal(CollectionProxyContext* context, HCollectionProxyComponent proxy,
                                                            ProxyLoadCallback cbk, void* cbk_ctx,
                                                            dmMessage::URL* receiver, dmMessage::Message* message)
    {
        if (proxy->m_Preloader != 0)
        {
            dmResource::DeletePreloader(proxy->m_Preloader);
            proxy->m_Preloader = 0;
        }
        if (proxy->m_Collection == 0)
        {
            LogMessageError(message, "The collection %s could not be unloaded since it was never loaded.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK;
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        dmResource::Release(context->m_Factory, proxy->m_Collection);
        proxy->m_Collection = 0;
        proxy->m_Initialized = 0;
        proxy->m_Enabled = 0;
        proxy->m_DelayedEnable = 0;
        proxy->m_Unloaded = 1;

        if (cbk)
        {
            proxy->m_Callback = cbk;
            proxy->m_CallbackCtx = cbk_ctx;
            dmMessage::ResetURL(&proxy->m_Unloader);
        }
        else
        {
            proxy->m_Unloader = *receiver;
        }

        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompCollectionProxyUnloadAsync(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx)
    {
        return CompCollectionProxyUnloadInternal(world->m_Context, proxy, cbk, cbk_ctx, 0, 0);
    }

    static dmGameObject::Result CompCollectionProxyInitializeInternal(HCollectionProxyComponent proxy, dmMessage::Message* message)
    {
        if (proxy->m_Collection != 0)
        {
            if (proxy->m_Initialized == 0)
            {
                dmGameObject::Init(proxy->m_Collection);
                proxy->m_Initialized = 1;
            }
            else
            {
                LogMessageError(message, "The collection %s could not be initialized since it has been already.", proxy->m_Resource->m_DDF->m_Collection);
                if (message)
                    return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
                return dmGameObject::RESULT_UNKNOWN_ERROR;
            }
        }
        else
        {
            LogMessageError(message, "The collection %s could not be initialized since it has not been loaded.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompCollectionProxyInitialize(HCollectionProxyWorld world, HCollectionProxyComponent proxy)
    {
        (void)world;
        return CompCollectionProxyInitializeInternal(proxy, 0);
    }

    static dmGameObject::Result CompCollectionProxyFinalizeInternal(HCollectionProxyComponent proxy, dmMessage::Message* message)
    {
        if (proxy->m_Initialized == 1 && proxy->m_Collection != 0x0)
        {
            dmGameObject::Final(proxy->m_Collection);
            proxy->m_Initialized = 0;
        }
        else
        {
            LogMessageError(message, "The collection %s could not be finalized since it was never initialized.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompCollectionProxyFinalize(HCollectionProxyWorld world, HCollectionProxyComponent proxy)
    {
        (void)world;
        return CompCollectionProxyFinalizeInternal(proxy, 0);
    }

    static dmGameObject::Result CompCollectionProxyEnableInternal(HCollectionProxyComponent proxy, dmMessage::Message* message)
    {
        if (proxy->m_Collection != 0)
        {
            if (proxy->m_Enabled == 0 && proxy->m_DelayedEnable == 0)
            {
                proxy->m_DelayedEnable = 1;

                if (proxy->m_Initialized == 0)
                {
                    dmGameObject::Init(proxy->m_Collection);
                    proxy->m_Initialized = 1;
                }
            }
            else
            {
                LogMessageError(message, "The collection %s could not be enabled since it is already.", proxy->m_Resource->m_DDF->m_Collection);
                if (message)
                    return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
                return dmGameObject::RESULT_UNKNOWN_ERROR;
            }
        }
        else
        {
            LogMessageError(message, "The collection %s could not be initialized since it has not been loaded.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompCollectionProxyEnable(HCollectionProxyWorld world, HCollectionProxyComponent proxy)
    {
        (void)world;
        return CompCollectionProxyEnableInternal(proxy, 0);
    }

    static dmGameObject::Result CompCollectionProxyDisableInternal(HCollectionProxyComponent proxy, dmMessage::Message* message)
    {
        if (proxy->m_Enabled == 1 && proxy->m_DelayedEnable == 1)
        {
            proxy->m_DelayedEnable = 0;
        }
        else
        {
            LogMessageError(message, "The collection %s could not be disabled since it is not enabled.", proxy->m_Resource->m_DDF->m_Collection);
            if (message)
                return dmGameObject::RESULT_OK; // The message code path doesn't catch errors
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompCollectionProxyDisable(HCollectionProxyWorld world, HCollectionProxyComponent proxy)
    {
        (void)world;
        return CompCollectionProxyDisableInternal(proxy, 0);
    }

    dmGameObject::Result CompCollectionProxySetTimeStep(HCollectionProxyWorld world, HCollectionProxyComponent proxy, float factor, int mode)
    {
        proxy->m_TimeStepFactor = factor < 0.0f ? 0.0f : factor;
        proxy->m_TimeStepMode = mode == 0 ? dmGameSystemDDF::TIME_STEP_MODE_CONTINUOUS : dmGameSystemDDF::TIME_STEP_MODE_DISCRETE;
    }

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*) *params.m_UserData;
        CollectionProxyContext* context = (CollectionProxyContext*)params.m_Context;

        if (params.m_Message->m_Id == COLLECTION_PROXY_LOAD_HASH || params.m_Message->m_Id == COLLECTION_PROXY_ASYNC_LOAD_HASH)
        {
            bool load_async = COLLECTION_PROXY_ASYNC_LOAD_HASH == params.m_Message->m_Id;
            dmGameObject::Result r = CompCollectionProxyLoadInternal(context, proxy, 0, 0,
                                            &params.m_Message->m_Sender, &params.m_Message->m_Receiver, params.m_Message, load_async);

            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == COLLECTION_PROXY_UNLOAD_HASH)
        {
            dmGameObject::Result r = CompCollectionProxyUnloadInternal(context, proxy, 0, 0, &params.m_Message->m_Sender, params.m_Message);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == COLLECTION_PROXY_INIT_HASH)
        {
            dmGameObject::Result r = CompCollectionProxyInitializeInternal(proxy, params.m_Message);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == COLLECTION_PROXY_FINAL_HASH)
        {
            dmGameObject::Result r = CompCollectionProxyFinalizeInternal(proxy, params.m_Message);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            dmGameObject::Result r = CompCollectionProxyEnableInternal(proxy, params.m_Message);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            dmGameObject::Result r = CompCollectionProxyDisableInternal(proxy, params.m_Message);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGameSystemDDF::SetTimeStep::m_DDFDescriptor)
        {
            dmGameSystemDDF::SetTimeStep* ddf = (dmGameSystemDDF::SetTimeStep*)params.m_Message->m_Data;
            dmGameObject::Result r = CompCollectionProxySetTimeStep(0, proxy, ddf->m_Factor, (int)ddf->m_Mode);
            return dmGameObject::RESULT_OK == r ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (params.m_Message->m_Id == dmHashString64("reset_time_step")) // DEPRECATED!
        {
            proxy->m_TimeStepFactor = 1.0f;
            proxy->m_TimeStepMode = dmGameSystemDDF::TIME_STEP_MODE_CONTINUOUS;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompCollectionProxyOnInput(const dmGameObject::ComponentOnInputParams& params)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*) *params.m_UserData;
        if (proxy->m_Enabled)
        {
            dmGameObject::InputAction* input_action = (dmGameObject::InputAction*)params.m_InputAction;
            dmGameObject::DispatchInput(proxy->m_Collection, input_action, 1);

            if (input_action->m_Consumed)
                return dmGameObject::INPUT_RESULT_CONSUMED;
        }
        return dmGameObject::INPUT_RESULT_IGNORED;
    }

    static bool CompCollectionProxyIterGetNext(dmGameObject::SceneNodeIterator* it)
    {
        it->m_Node = it->m_NextChild; // copy data fields
        it->m_NextChild.m_Node = 0; // we only have one collection to worry about
        return it->m_Node.m_Node != 0;
    }

    void CompCollectionProxyIterChildren(dmGameObject::SceneNodeIterator* it, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        CollectionProxyComponent* proxy = (CollectionProxyComponent*)node->m_Component;
        it->m_Parent = *node;
        it->m_NextChild = *node; // copy data fields
        it->m_NextChild.m_Collection = proxy->m_Collection;
        it->m_NextChild.m_Type = dmGameObject::SCENE_NODE_TYPE_COLLECTION;
        it->m_NextChild.m_Node = (uint64_t)proxy->m_Collection;
        it->m_FnIterateNext = CompCollectionProxyIterGetNext;
    }

    /*# tells a collection proxy to start loading the referenced collection
     *
     * Post this message to a collection-proxy-component to start the loading of the referenced collection.
     * When the loading has completed, the message [ref:proxy_loaded] will be sent back to the script.
     *
     * A loaded collection must be initialized (message [ref:init]) and enabled (message [ref:enable]) in order to be simulated and drawn.
     *
     * @message
     * @name load
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to start asynchronous loading of the referenced collection
     *
     * Post this message to a collection-proxy-component to start background loading of the referenced collection.
     * When the loading has completed, the message [ref:proxy_loaded] will be sent back to the script.
     *
     * A loaded collection must be initialized (message [ref:init]) and enabled (message [ref:enable]) in order to be simulated and drawn.
     *
     * @message
     * @name async_load
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "async_load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# reports that a collection proxy has loaded its referenced collection
     *
     * This message is sent back to the script that initiated a collection proxy load when the referenced
     * collection is loaded. See documentation for [ref:load] for examples how to use.
     *
     * @message
     * @name proxy_loaded
     */

    /*# tells a collection proxy to initialize the loaded collection
     * Post this message to a collection-proxy-component to initialize the game objects and components in the referenced collection.
     * Sending [ref:enable] to an uninitialized collection proxy automatically initializes it.
     * The [ref:init] message simply provides a higher level of control.
     *
     * @message
     * @name init
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("load_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- only initialize the proxy at this point since we want to enable it at a later time for some reason
     *         msg.post(sender, "init")
     *         -- let loader know
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to enable the referenced collection
     * Post this message to a collection-proxy-component to enable the referenced collection, which in turn enables the contained game objects and components.
     * If the referenced collection was not initialized prior to this call, it will automatically be initialized.
     *
     * @message
     * @name enable
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, "level_started")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to disable the referenced collection
     * Post this message to a collection-proxy-component to disable the referenced collection, which in turn disables the contained game objects and components.
     *
     * @message
     * @name disable
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to finalize the referenced collection
     * Post this message to a collection-proxy-component to finalize the referenced collection, which in turn finalizes the contained game objects and components.
     *
     * @message
     * @name final
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to start unloading the referenced collection
     *
     * Post this message to a collection-proxy-component to start the unloading of the referenced collection.
     * When the unloading has completed, the message [ref:proxy_unloaded] will be sent back to the script.
     *
     * @message
     * @name unload
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# reports that a collection proxy has unloaded its referenced collection
     *
     * This message is sent back to the script that initiated an unload with a collection proxy when
     * the referenced collection is unloaded. See documentation for [ref:unload] for examples how to use.
     *
     * @message
     * @name proxy_unloaded
     */

    /*# return an indexed table of all the resources of a collection proxy
     *
     * return an indexed table of resources for a collection proxy. Each
     * entry is a hexadecimal string that represents the data of the specific
     * resource. This representation corresponds with the filename for each
     * individual resource that is exported when you bundle an application with
     * LiveUpdate functionality.
     *
     * @namespace collectionproxy
     * @name collectionproxy.get_resources
     * @param collectionproxy [type:url] the collectionproxy to check for resources.
     * @return resources [type:table] the resources
     *
     * @examples
     *
     * ```lua
     * local function print_resources(self, cproxy)
     *     local resources = collectionproxy.get_resources(cproxy)
     *     for _, v in ipairs(resources) do
     *         print("Resource: " .. v)
     *     end
     * end
     * ```
     */

    /*# return an array of missing resources for a collection proxy
     *
     * return an array of missing resources for a collection proxy. Each
     * entry is a hexadecimal string that represents the data of the specific
     * resource. This representation corresponds with the filename for each
     * individual resource that is exported when you bundle an application with
     * LiveUpdate functionality. It should be considered good practise to always
     * check whether or not there are any missing resources in a collection proxy
     * before attempting to load the collection proxy.
     *
     * @namespace collectionproxy
     * @name collectionproxy.missing_resources
     * @param collectionproxy [type:url] the collectionproxy to check for missing
     * resources.
     * @return resources [type:table] the missing resources
     *
     * @examples
     *
     * ```lua
     * function init(self)
     * end
     *
     * local function callback(self, id, response)
     *     local expected = self.resources[id]
     *     if response ~= nil and response.status == 200 then
     *         print("Successfully downloaded resource: " .. expected)
     *         resource.store_resource(response.response)
     *     else
     *         print("Failed to download resource: " .. expected)
     *         -- error handling
     *     end
     * end
     *
     * local function download_resources(self, cproxy)
     *     self.resources = {}
     *     local resources = collectionproxy.missing_resources(cproxy)
     *     for _, v in ipairs(resources) do
     *         print("Downloading resource: " .. v)
     *
     *         local uri = "http://example.defold.com/" .. v
     *         local id = http.request(uri, "GET", callback)
     *         self.resources[id] = v
     *     end
     * end
     * ```
     */
}
