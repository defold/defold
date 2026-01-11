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

#ifndef DM_GAMESYS_COMP_PRIVATE_H
#define DM_GAMESYS_COMP_PRIVATE_H

#include <dlib/hash.h>
#include <gameobject/gameobject.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gamesys/property.h>
#include <dmsdk/gamesys/render_constants.h>

namespace dmGameSystem
{
    dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const dmVMath::Vector3& ref_value, const PropVector3& property);
    dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, dmVMath::Vector3& set_value, const PropVector3& property);

    dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const dmVMath::Vector4& ref_value, const PropVector4& property);
    dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, dmVMath::Vector4& set_value, const PropVector4& property);

    // Render constants
    void CopyRenderConstants(HComponentRenderConstants dst, HComponentRenderConstants src);
}

#endif // DM_GAMESYS_COMP_PRIVATE_H
