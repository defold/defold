// Copyright 2020-2025 The Defold Foundation
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import java.util.List;

import com.dynamo.bob.Task;
import com.dynamo.render.proto.Compute;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.google.protobuf.Message;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;

public class CollisionObjectTest extends AbstractProtoBuilderTest {

    private String collisionObjectInput =
            """
                type: COLLISION_OBJECT_TYPE_DYNAMIC
                mass: 1.0
                friction: 0.1
                restitution: 0.5
                group: "default"
                mask: "default"
                embedded_collision_shape {
                  shapes {
                    shape_type: TYPE_BOX
                    position {
                    }
                    rotation {
                    }
                    index: 0
                    count: 3
                  }
                  data: 10.0
                  data: 10.0
                  data: 10.0
                }
                """;

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testSelectBox2DBuilder() throws Exception {
        this.getProject().getProjectProperties().putStringValue("physics", "type", "2D");

        Task task = getTask("/empty.collisionobject", collisionObjectInput);
        assertTrue(task.getBuilder().getClass().isAssignableFrom(CollisionObjectBox2DBuilder.class));

        List<Message> outputs = build(task);
        assertEquals(1, outputs.size());
    }

    @Test
    public void testSelectBullet3DBuilder() throws Exception {
        this.getProject().getProjectProperties().putStringValue("physics", "type", "3D");

        Task task = getTask("/empty.collisionobject", collisionObjectInput);
        assertTrue(task.getBuilder().getClass().isAssignableFrom(CollisionObjectBullet3DBuilder.class));

        List<Message> outputs = build(task);
        assertEquals(1, outputs.size());
    }
}
