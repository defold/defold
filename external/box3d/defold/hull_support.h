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

#ifndef DM_BOX3D_HULL_SUPPORT_H
#define DM_BOX3D_HULL_SUPPORT_H
#include <box3d/collision.h>

enum
{
    DM_PREPARED_FACE_LIMIT = 32
};
typedef struct dmBox3DHullSupport
{
    uint8_t m_OnA[DM_PREPARED_FACE_LIMIT];
    uint8_t m_OnB[DM_PREPARED_FACE_LIMIT];
    uint8_t m_PrismAxisPlusOne;
} dmBox3DHullSupport;

void dmBox3DPrepareHullSupport(dmBox3DHullSupport* support, const b3HullData* a, const b3HullData* b, b3Quat rotation);
bool dmBox3DCollidePreparedHulls(const dmBox3DHullSupport* support, b3Quat prepared_rotation, b3LocalManifold* manifold,
                                 int capacity, const b3HullData* a, const b3HullData* b, b3Transform pose, b3SATCache* axis);
#endif
