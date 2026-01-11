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

package com.dynamo.bob.pipeline;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class GameObjectBuilderTest extends AbstractProtoBuilderTest {

    @Test
    public void testProps() throws Exception {
        addFile("/test.script", "");
        StringBuilder src = new StringBuilder();
        src.append("components {");
        src.append("  id: \"script\"\n");
        src.append("  component: \"/test.script\"\n");
        src.append("  properties { id: \"number\" value: \"1\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("  properties { id: \"hash\" value: \"hash\" type: PROPERTY_TYPE_HASH }\n");
        src.append("  properties { id: \"url\" value: \"url\" type: PROPERTY_TYPE_URL }\n");
        src.append("  properties { id: \"vec3\" value: \"1, 2, 3\" type: PROPERTY_TYPE_VECTOR3 }\n");
        src.append("  properties { id: \"vec4\" value: \"4, 5, 6, 7\" type: PROPERTY_TYPE_VECTOR4 }\n");
        src.append("  properties { id: \"quat\" value: \"8, 9, 10, 11\" type: PROPERTY_TYPE_QUAT }\n");
        src.append("  properties { id: \"bool\" value: \"true\" type: PROPERTY_TYPE_BOOLEAN }\n");
        src.append("}\n");
        PrototypeDesc prototype = getMessage(build("/test.go", src.toString()), PrototypeDesc.class);
        for (ComponentDesc cd : prototype.getComponentsList()) {
            PropertyDeclarations properties = cd.getPropertyDecls();
            PropertiesTestUtil.assertNumber(properties, 1, 0);
            PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
            PropertiesTestUtil.assertURL(properties, "url", 0);
            PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
            PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
            PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
            PropertiesTestUtil.assertBoolean(properties, true, 0);
        }
    }

    @Test(expected = CompileExceptionError.class)
    public void testPropInvalidValue() throws Exception {
        addFile("/test.script", "");
        StringBuilder src = new StringBuilder();
        src.append("components {");
        src.append("  id: \"script\"\n");
        src.append("  component: \"/test.script\"\n");
        src.append("  properties { id: \"number\" value: \"a\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("}\n");
        @SuppressWarnings("unused")
        PrototypeDesc prototype = (PrototypeDesc)build("/test.go", src.toString()).get(0);
    }
}
