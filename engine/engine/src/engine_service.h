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

#ifndef DM_ENGINE_SERVICE
#define DM_ENGINE_SERVICE

#include <stdint.h>

namespace dmResource
{
	struct SResourceFactory;
	typedef struct SResourceFactory* HFactory;
}

namespace dmGameObject
{
    typedef struct Register* HRegister;
}

namespace dmProfile
{
    typedef struct Profile* HProfile;
}

namespace dmEngineService
{
    typedef struct EngineService* HEngineService;

    // Parses the environment flag DM_SERVICE_PORT=xxxx for a port, or returns the default port
    uint16_t GetServicePort(uint16_t default_port);

    HEngineService New(uint16_t port);
    void Delete(HEngineService engine_service);
    void Update(HEngineService engine_service, dmProfile::HProfile);
    uint16_t GetPort(HEngineService engine_service);

    void InitProfiler(HEngineService engine_service, dmResource::HFactory factory, dmGameObject::HRegister regist);
}

#endif // DM_ENGINE_SERVICE
