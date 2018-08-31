#include "gameobject_private.h"
#include <resource/resource.h>
#include <dlib/mutex.h>

namespace dmGameObject
{

bool IterateCollections(HRegister regist, FCollectionIterator callback, void* user_ctx)
{
    for(uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
    {
        Collection* collection = regist->m_Collections[i];
        DM_MUTEX_SCOPED_LOCK(collection->m_Mutex);
        if(!collection->m_ToBeDeleted)
        {
            IteratorCollection iterator;
            iterator.m_Collection = collection->m_HCollection;
            iterator.m_NameHash = collection->m_NameHash;
            iterator.m_Resource = 0;
            dmResource::GetPath(collection->m_Factory, collection, &iterator.m_Resource);
            if (!callback(&iterator, user_ctx))
                return false;
        }
    }
    return true;
}

static bool IterateGameObject(Collection* collection, HInstance instance, FGameObjectIterator callback, void* user_ctx)
{
    IteratorGameObject iterator;
    iterator.m_Collection = collection->m_HCollection;
    iterator.m_Instance = instance;
    iterator.m_NameHash = instance->m_Identifier;
    iterator.m_Resource = 0;
    dmResource::GetPath(dmGameObject::GetFactory(collection->m_HCollection), instance->m_Prototype, &iterator.m_Resource);

    if (!callback(&iterator, user_ctx))
        return false;

    uint32_t childIndex = instance->m_FirstChildIndex;
    while (childIndex != INVALID_INSTANCE_INDEX)
    {
        Instance* child = collection->m_Instances[childIndex];
        assert(child->m_Parent == instance->m_Index);
        childIndex = child->m_SiblingIndex;
        if (!IterateGameObject(collection, child, callback, user_ctx))
            return false;
    }
    return true;
}

bool IterateGameObjects(HCollection hcollection, FGameObjectIterator callback, void* user_ctx)
{
    Collection* collection = hcollection->m_Collection;
    const dmArray<uint16_t>& root_level = collection->m_LevelIndices[0];
    for (uint32_t j = 0; j < root_level.Size(); ++j)
    {
        if (!IterateGameObject(collection, collection->m_Instances[root_level[j]], callback, user_ctx))
            return false;
    }
    return true;
}

bool IterateComponents(HInstance instance, FGameComponentIterator callback, void* user_ctx)
{
    Prototype* prototype = instance->m_Prototype;
    for (uint32_t k = 0; k < prototype->m_Components.Size(); ++k)
    {
        Prototype::Component* component = &prototype->m_Components[k];

        IteratorComponent iterator;
        iterator.m_Collection = GetCollection(instance);
        iterator.m_Instance = instance;
        iterator.m_NameHash = component->m_Id;
        iterator.m_Type     = component->m_Type->m_Name;
        iterator.m_Resource = component->m_ResourceId;
        if (!callback(&iterator, user_ctx))
            return false;
    }
    return true;
}

}
