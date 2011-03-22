#include "comp_collection_proxy.h"
#include "resources/res_collection_proxy.h"

#include <string.h>

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>

#include <gameobject/gameobject.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    struct CollectionProxyComponent
    {
        CollectionProxyResource*        m_Resource;
        dmGameObject::HCollection       m_Collection;
        dmGameSystemDDF::TimeStepMode   m_TimeStepMode;
        float                           m_TimeStepFactor;
        float                           m_AccumulatedTime;
        uint32_t                        m_Initialized : 1;
        uint32_t                        m_Enabled : 1;
        uint32_t                        m_Unload : 1;
    };

    struct CollectionProxyWorld
    {
        dmArray<CollectionProxyComponent>   m_Components;
        dmIndexPool32                       m_IndexPool;
    };

    dmGameObject::CreateResult CompCollectionProxyNewWorld(void* context, void** world)
    {
        CollectionProxyWorld* proxy_world = new CollectionProxyWorld();
        // TODO: tweak count from project-file
        const uint32_t component_count = 8;
        proxy_world->m_Components.SetCapacity(component_count);
        proxy_world->m_Components.SetSize(component_count);
        memset(&proxy_world->m_Components[0], 0, sizeof(CollectionProxyComponent) * component_count);
        proxy_world->m_IndexPool.SetCapacity(component_count);
        *world = proxy_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(void* context, void* world)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)world;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            dmGameObject::HCollection collection = proxy_world->m_Components[i].m_Collection;
            if (collection != 0)
            {
                if (proxy_world->m_Components[i].m_Initialized)
                    dmGameObject::Final(collection);
                dmResource::Release((dmResource::HFactory)context, collection);
            }
        }
        delete proxy_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionProxyCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)world;
        if (proxy_world->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = proxy_world->m_IndexPool.Pop();
            CollectionProxyComponent* proxy = &proxy_world->m_Components[index];
            memset(proxy, 0, sizeof(CollectionProxyComponent));
            proxy->m_TimeStepFactor = 1.0f;
            proxy->m_Resource = (CollectionProxyResource*) resource;
            *user_data = (uintptr_t) proxy;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Collection spawn point could not be created since the buffer is full (%ud).", proxy_world->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCollectionProxyDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*)*user_data;
        if (proxy->m_Collection != 0)
        {
            if (proxy->m_Initialized)
                dmGameObject::Final(proxy->m_Collection);
            dmResource::Release((dmResource::HFactory)context, proxy->m_Collection);
        }
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)world;
        uint32_t index = proxy - &proxy_world->m_Components[0];
        proxy_world->m_IndexPool.Push(index);
        memset(proxy, 0, sizeof(CollectionProxyComponent));
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollectionProxyUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)world;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < proxy_world->m_Components.Size(); ++i)
        {
            CollectionProxyComponent* proxy = &proxy_world->m_Components[i];
            if (proxy->m_Collection != 0)
            {
                if (proxy->m_Enabled)
                {
                    dmGameObject::UpdateContext uc;

                    float warped_dt = update_context->m_DT * proxy->m_TimeStepFactor;
                    switch (proxy->m_TimeStepMode)
                    {
                    case dmGameSystemDDF::TIME_STEP_MODE_CONTINUOUS:
                        uc.m_DT = warped_dt;
                        proxy->m_AccumulatedTime = 0.0f;
                        break;
                    case dmGameSystemDDF::TIME_STEP_MODE_DISCRETE:
                        proxy->m_AccumulatedTime += warped_dt;
                        if (proxy->m_AccumulatedTime >= update_context->m_DT)
                        {
                            uc.m_DT = update_context->m_DT;
                            proxy->m_AccumulatedTime -= update_context->m_DT;
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
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(dmGameObject::HCollection collection,
                                               void* world,
                                               void* context)
    {
        CollectionProxyWorld* proxy_world = (CollectionProxyWorld*)world;
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
                if (proxy->m_Unload)
                {
                    if (proxy->m_Initialized)
                    {
                        dmGameObject::Final(proxy->m_Collection);
                        proxy->m_Initialized = 0;
                    }
                    dmResource::Release((dmResource::HFactory)context, proxy->m_Collection);
                    proxy->m_Collection = 0;
                    proxy->m_Enabled = 0;
                    proxy->m_Unload = 0;
                }
            }
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*) *user_data;
        if (message_data->m_MessageId == dmHashString64("load"))
        {
            if (proxy->m_Collection == 0)
            {
                proxy->m_Unload = 0;
                // TODO: asynchronous loading
                dmResource::FactoryResult result = dmResource::Get((dmResource::HFactory)context, proxy->m_Resource->m_DDF->m_Collection, (void**)&proxy->m_Collection);
                if (result != dmResource::FACTORY_RESULT_OK)
                {
                    dmLogError("The collection %s could not be loaded.", proxy->m_Resource->m_DDF->m_Collection);
                    return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                }
            }
            else
            {
                dmLogWarning("The collection %s could not loaded since it was already.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("unload"))
        {
            if (proxy->m_Collection != 0)
            {
                proxy->m_Unload = 1;
            }
            else
            {
                dmLogWarning("The collection %s could not be unloaded since it was never loaded.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("init"))
        {
            if (proxy->m_Initialized == 0)
            {
                dmGameObject::Init(proxy->m_Collection);
                proxy->m_Initialized = 1;
            }
            else
            {
                dmLogWarning("The collection %s could not be initialized since it has been already.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("final"))
        {
            if (proxy->m_Initialized == 1)
            {
                dmGameObject::Final(proxy->m_Collection);
                proxy->m_Initialized = 0;
            }
            else
            {
                dmLogWarning("The collection %s could not be finalized since it was never initialized.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("enable"))
        {
            if (proxy->m_Enabled == 0)
            {
                proxy->m_Enabled = 1;
                if (proxy->m_Initialized == 0)
                {
                    dmGameObject::Init(proxy->m_Collection);
                    proxy->m_Initialized = 1;
                }
            }
            else
            {
                dmLogWarning("The collection %s could not be enabled since it is already.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("disable"))
        {
            if (proxy->m_Enabled == 1)
            {
                proxy->m_Enabled = 0;
            }
            else
            {
                dmLogWarning("The collection %s could not be disabled since it is not enabled.", proxy->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_DDFDescriptor == dmGameSystemDDF::SetTimeStep::m_DDFDescriptor)
        {
            dmGameSystemDDF::SetTimeStep* ddf = (dmGameSystemDDF::SetTimeStep*)message_data->m_Buffer;
            proxy->m_TimeStepFactor = ddf->m_Factor;
            proxy->m_TimeStepMode = ddf->m_Mode;
        }
        else if (message_data->m_MessageId == dmHashString64("reset_time_step"))
        {
            proxy->m_TimeStepFactor = 1.0f;
            proxy->m_TimeStepMode = dmGameSystemDDF::TIME_STEP_MODE_CONTINUOUS;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompCollectionProxyOnInput(dmGameObject::HInstance instance,
            const dmGameObject::InputAction* input_action,
            void* context,
            uintptr_t* user_data)
    {
        CollectionProxyComponent* proxy = (CollectionProxyComponent*) *user_data;
        if (proxy->m_Enabled && !proxy->m_Unload)
            dmGameObject::DispatchInput(proxy->m_Collection, (dmGameObject::InputAction*)input_action, 1);
        return dmGameObject::INPUT_RESULT_IGNORED;
    }
}
