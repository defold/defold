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

#include <dmsdk/resource/resource.hpp>

namespace dmGameObject
{
    static dmResource::Result ResAnimCreate(const dmResource::ResourceCreateParams* params)
    {
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    static dmResource::Result ResAnimDestroy(const dmResource::ResourceDestroyParams* params)
    {
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    static ResourceResult RegisterResourceTypeAnim(HResourceTypeContext ctx, HResourceType type)
    {
        return (ResourceResult)dmResource::SetupType(ctx, type, 0, 0, ResAnimCreate, 0, ResAnimDestroy, 0);
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeAnim, "animc", dmGameObject::RegisterResourceTypeAnim, 0);
