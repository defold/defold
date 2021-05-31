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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;

import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.google.protobuf.Message;

public class SpineModelBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testSpineModel() throws Exception {
        addFile("/test.spinescene", "");
        addFile("/test.material", "");
        StringBuilder src = new StringBuilder();
        src.append("spine_scene: \"/test.spinescene\"");
        src.append(" material: \"/test.material\"");
        src.append(" default_animation: \"test\"");
        src.append(" skin: \"test\"");
        src.append(" blend_mode: BLEND_MODE_ALPHA");
        List<Message> outputs = build("/test.spinemodel", src.toString());

        Class<?> klass = this.getClass("com.dynamo.spine.proto.Spine$SpineModelDesc");
        Class<?> klassBlendMode = this.getClass("com.dynamo.spine.proto.Spine$SpineModelDesc$BlendMode");
        Object scene = (Object)outputs.get(0);

        assertEquals("/test.rigscenec", (String)callFunction(scene, "getSpineScene"));
        assertEquals("/test.materialc", (String)callFunction(scene, "getMaterial"));
        assertEquals("test", (String)callFunction(scene, "getDefaultAnimation"));
        assertEquals("test", (String)callFunction(scene, "getSkin"));
        Object blendMode = callFunction(scene, "getBlendMode");
        assertEquals((int)callFunction(getField(klassBlendMode, null, "BLEND_MODE_ALPHA"), "getNumber"), (int)callFunction(blendMode, "getNumber"));
    }
}
