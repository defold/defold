#include "comp_collection_factory.h"
#include "resources/res_collection_factory.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const char* COLLECTION_FACTORY_MAX_COUNT_KEY = "collectionfactory.max_count";

    struct FactoryWorld
    {
        dmArray<CollectionFactoryComponent>   m_Components;
        dmIndexPool32               m_IndexPool;
        uint32_t                    m_TotalFactoryCount;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        FactoryContext* context = (FactoryContext*)params.m_Context;
        FactoryWorld* fw = new FactoryWorld();
        const uint32_t max_component_count = context->m_MaxFactoryCount;
        fw->m_Components.SetCapacity(max_component_count);
        fw->m_Components.SetSize(max_component_count);
        fw->m_IndexPool.SetCapacity(max_component_count);
        memset(&fw->m_Components[0], 0, sizeof(CollectionFactoryComponent) * max_component_count);
        *params.m_World = fw;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (FactoryWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        if (fw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = fw->m_IndexPool.Pop();
            CollectionFactoryComponent* fc = &fw->m_Components[index];
            fc->m_Resource = (CollectionFactoryResource*) params.m_Resource;
            *params.m_UserData = (uintptr_t) fc;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Can not create more collection factory components since the buffer is full (%d).", fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        CollectionFactoryComponent* fc = (CollectionFactoryComponent*)*params.m_UserData;
        uint32_t index = fc - &fw->m_Components[0];
        fc->m_Resource = 0x0;
        fw->m_IndexPool.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }
}
