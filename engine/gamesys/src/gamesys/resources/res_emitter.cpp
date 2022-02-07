// Copyright 2020 The Defold Foundation
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

#include "res_emitter.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{

    dmResource::Result ResEmitterCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLogWarning("%s will not be loaded since emitter files are deprecated", params.m_Filename);
        // Trick resource system into thinking we loaded something
        params.m_Resource->m_Resource = (void*)1;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterDestroy(const dmResource::ResourceDestroyParams& params)
    {
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterRecreate(const dmResource::ResourceRecreateParams& params)
    {
        return dmResource::RESULT_OK;
    }
}
