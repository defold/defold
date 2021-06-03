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

#ifndef DM_GAMESYS_COMP_PRIVATE_H
#define DM_GAMESYS_COMP_PRIVATE_H

#include <dlib/hash.h>
#include <dlib/math.h>
#include <gameobject/gameobject.h>
#include <render/render.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/gamesys/property.h>

namespace dmGameSystem
{
    const uint32_t MAX_COMP_RENDER_CONSTANTS = 16;

    dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const Vectormath::Aos::Vector3& ref_value, const PropVector3& property);
    dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vectormath::Aos::Vector3& set_value, const PropVector3& property);

    dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const Vectormath::Aos::Vector4& ref_value, const PropVector4& property);
    dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vectormath::Aos::Vector4& set_value, const PropVector4& property);

    struct CompRenderConstants
    {
        CompRenderConstants();
        dmArray<dmRender::Constant> m_RenderConstants;
        dmArray<dmVMath::Vector4>   m_PrevRenderConstants;
    };
}

#endif // DM_GAMESYS_COMP_PRIVATE_H
