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

package com.dynamo.bob.test.util;

import javax.vecmath.Quat4d;

import com.dynamo.bob.util.RigUtil.RotationBuilder;
import com.dynamo.rig.proto.Rig.AnimationTrack.Builder;

public class MockRotationBuilder extends RotationBuilder {

    public MockRotationBuilder(Builder builder) {
        super(builder);
    }

    public int GetRotationsCount() {
        return builder.getRotationsCount();
    }
    
    public Quat4d GetRotations(int index) {
        Quat4d rot = new Quat4d(
                builder.getRotations(index),
                builder.getRotations(index+1),
                builder.getRotations(index+2),
                builder.getRotations(index+3));
        return rot;
    }
}
