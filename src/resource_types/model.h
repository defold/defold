#ifndef RESOURCE_MODEL_H
#define RESOURCE_MODEL_H

#include <gamesys/gameobject.h>
#include <gamesys/resource.h>

namespace dmModel
{
    dmResource::CreateResult CreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult DestroyResource(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult RecreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmGameObject::CreateResult CreateComponent(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult DestroyComponent(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* context,
            uintptr_t* user_data);

    dmGameObject::UpdateResult UpdateComponent(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* context);

    dmGameObject::UpdateResult OnEventComponent(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data);
}

#endif
