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

#ifndef DM_PHYSICS_BOX3D_PRIVATE_H
#define DM_PHYSICS_BOX3D_PRIVATE_H

#include <box3d/box3d.h>

namespace dmPhysics
{
    float GetBox3DScale(b3WorldId world);
    void  SetBox3DComponentEvents(b3WorldId world, bool enabled);
    void  SetBox3DPaused(b3WorldId world, bool paused);
    void  PrepareBox3DSleepingContacts(b3WorldId world);
    void  SetBox3DAwake(b3BodyId body, bool awake);
    void  SetBox3DTransform(b3BodyId body, b3Pos position, b3Quat rotation);
}

#endif
