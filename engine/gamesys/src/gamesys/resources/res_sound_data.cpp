// Copyright 2020-2022 The Defold Foundation
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

#include <string.h>
#include <sound/sound.h>
#include "res_sound_data.h"

namespace dmGameSystem
{
    dmResource::Result ResSoundDataCreate(const dmResource::ResourceCreateParams& params)
    {
        dmSound::HSoundData sound_data;

        dmSound::SoundDataType type = dmSound::SOUND_DATA_TYPE_WAV;

        size_t filename_len = strlen(params.m_Filename);
        if (filename_len > 5 && strcmp(params.m_Filename + filename_len - 5, ".oggc") == 0)
        {
            type = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
        }

        dmSound::Result r = dmSound::NewSoundData(params.m_Buffer, params.m_BufferSize, type, &sound_data, params.m_Resource->m_NameHash);
        if (r != dmSound::RESULT_OK)
        {
            return dmResource::RESULT_OUT_OF_RESOURCES;
        }

        dmSound::SoundDataResource* sound_data_res = new dmSound::SoundDataResource();

        sound_data_res->m_SoundData = sound_data;

        params.m_Resource->m_Resource = (void*) sound_data_res;
        params.m_Resource->m_ResourceSize = dmSound::GetSoundResourceSize(sound_data_res);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundDataDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmSound::SoundDataResource* sound_data_res = (dmSound::SoundDataResource*) params.m_Resource->m_Resource;
        dmSound::Result r = dmSound::DeleteSoundData(sound_data_res->m_SoundData);
        if (r != dmSound::RESULT_OK)
        {
            return dmResource::RESULT_INVAL;
        }
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundDataRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmSound::SoundDataResource* sound_data_res = (dmSound::SoundDataResource*) params.m_Resource->m_Resource;

        dmSound::HSoundData sound_data;
        dmSound::SoundDataType type = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
        dmSound::Result r = dmSound::NewSoundData(params.m_Buffer, params.m_BufferSize, type, &sound_data, params.m_Resource->m_NameHash);

        if (r != dmSound::RESULT_OK)
        {
            return dmResource::RESULT_INVAL;
        }
        sound_data_res->m_SoundData = sound_data;

        params.m_Resource->m_Resource = (void*)sound_data_res;
        params.m_Resource->m_ResourceSize = dmSound::GetSoundResourceSize(sound_data_res);
        return dmResource::RESULT_OK;
    }
}
