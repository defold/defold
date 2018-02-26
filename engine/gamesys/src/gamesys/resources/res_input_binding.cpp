#include "res_input_binding.h"

#include <ddf/ddf.h>

#include <input/input.h>

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResInputBindingCreate(const dmResource::ResourceCreateParams& params)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = dmInput::NewBinding((dmInput::HContext) params.m_Context);
        dmInput::SetBinding(binding, ddf);
        params.m_Resource->m_Resource = (void*)binding;
        dmDDF::FreeMessage((void*)ddf);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmInput::DeleteBinding((dmInput::HBinding)params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = (dmInput::HBinding)params.m_Resource->m_Resource;
        dmInput::SetBinding(binding, ddf);
        dmDDF::FreeMessage((void*)ddf);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        params.m_DataSize = dmInput::GetBindingResourceSize((dmInput::HBinding)params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

}
