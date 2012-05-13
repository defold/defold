#include "res_script.h"

#include "gameobject_script.h"

namespace dmGameObject
{
    dmResource::Result ResScriptCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        HScript script = NewScript(buffer, buffer_size, filename);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResScriptDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        DeleteScript((HScript) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptRecreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename)
    {
        HScript script = (HScript) resource->m_Resource;
        if (ReloadScript(script, buffer, buffer_size, filename))
        {
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
