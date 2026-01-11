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

#ifndef GAME_PHYSICS_DEBUG_RENDER_H
#define GAME_PHYSICS_DEBUG_RENDER_H

#include <stdint.h>
#include <dmsdk/dlib/vmath.h>

namespace PhysicsDebugRender
{
    void DrawLines(dmVMath::Point3* points, uint32_t point_count, dmVMath::Vector4 color, void* user_data);
    void DrawTriangles(dmVMath::Point3* points, uint32_t point_count, dmVMath::Vector4 color, void* user_data);
}

#endif
