#include "res_sprite.h"

#include <dlib/log.h>

#include "sprite_ddf.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResSpriteCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        dmGameSystemDDF::SpriteDesc* sprite_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &sprite_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        if (sprite_desc->m_ColumnCount == 0 || sprite_desc->m_RowCount == 0)
        {
            dmLogError("%s", "column_count and row_count must both be over 0");
            dmDDF::FreeMessage(sprite_desc);
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::SpriteDesc** sprite_resource = new dmGameSystemDDF::SpriteDesc*;
        *sprite_resource = sprite_desc;
        resource->m_Resource = (void*) sprite_resource;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSpriteDestroy(dmResource::HFactory factory,
                                             void* context,
                                             dmResource::SResourceDescriptor* resource)
    {
        dmGameSystemDDF::SpriteDesc** sprite_resource = (dmGameSystemDDF::SpriteDesc**) resource->m_Resource;
        dmDDF::FreeMessage(*sprite_resource);
        delete sprite_resource;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSpriteRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename)
    {
        dmGameSystemDDF::SpriteDesc* sprite_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &sprite_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        if (sprite_desc->m_ColumnCount == 0 || sprite_desc->m_RowCount == 0)
        {
            dmLogError("%s", "column_count and row_count must both be over 0");
            dmDDF::FreeMessage(sprite_desc);
            return dmResource::CREATE_RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::SpriteDesc** sprite_resource = (dmGameSystemDDF::SpriteDesc**) resource->m_Resource;
        dmDDF::FreeMessage(*sprite_resource);
        *sprite_resource = sprite_desc;
        return dmResource::CREATE_RESULT_OK;
    }
}
