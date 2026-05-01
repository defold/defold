// Copyright 2020-2026 The Defold Foundation
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

#include "engine_service.h"

dmEngineService::HEngineService dmEngineService::New(uint16_t)
{
	return 0;
}

void dmEngineService::Delete(dmEngineService::HEngineService)
{
}

void dmEngineService::Update(dmEngineService::HEngineService, HProfile)
{
}

uint16_t dmEngineService::GetPort(dmEngineService::HEngineService)
{
	return 0;
}

uint16_t dmEngineService::GetServicePort(uint16_t default_port)
{
	return default_port;
}

dmWebServer::HServer dmEngineService::GetWebServer(dmEngineService::HEngineService)
{
    return 0;
}

void dmEngineService::InitProfiler(HEngineService engine_service, dmResource::HFactory factory, dmGameObject::HRegister regist)
{
}

void dmEngineService::InitState(HEngineService engine_service, EngineState* state)
{	
}
