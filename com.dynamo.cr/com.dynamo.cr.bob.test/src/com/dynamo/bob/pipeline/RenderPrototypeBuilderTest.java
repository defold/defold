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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.google.protobuf.Message;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;

public class RenderPrototypeBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testDataMigration() throws Exception {
        addFile("/test.render_script", "");

        StringBuilder srcShader = new StringBuilder();
        srcShader.append("void main() {}\n");

        addFile("/testDataMigration.vp", srcShader.toString());
        addFile("/testDataMigration.fp", srcShader.toString());

        StringBuilder materialSrc = new StringBuilder();
        materialSrc.append("name: \"test_material\"\n");
        materialSrc.append("vertex_program: \"/testDataMigration.vp\"\n");
        materialSrc.append("fragment_program: \"/testDataMigration.fp\"\n");
        addFile("/test.material", materialSrc.toString());

        {
            final String srcOneMaterial =
                "script: \"/test.render_script\"\n" +
                "materials {\n" +
                "   name: \"test\"\n" +
                "   material: \"/test.material\"\n" +
                "}\n";

            List<Message> outputs = build("/test.render", srcOneMaterial);
            RenderPrototypeDesc output = getMessage(outputs, RenderPrototypeDesc.class);

            assertEquals(0, output.getMaterialsList().size());
            assertEquals(1, output.getRenderResourcesList().size());

            RenderPrototypeDesc.RenderResourceDesc res_desc = output.getRenderResourcesList().get(0);
            assertTrue(res_desc.getName().equals("test"));
            assertTrue(res_desc.getPath().equals("/test.materialc"));
        }

        {
            final String srcOneMaterialOneRenderResource =
                "script: \"/test.render_script\"\n" +
                "materials {\n" +
                "   name: \"test\"\n" +
                "   material: \"/test.material\"\n" +
                "}\n" +
                "render_resources {\n" +
                "   name: \"test_2\"\n" +
                "   path: \"/test.material\"\n" +
                "}\n";

            RenderPrototypeDesc output = getMessage(build("/test2.render", srcOneMaterialOneRenderResource), RenderPrototypeDesc.class);

            assertEquals(0, output.getMaterialsList().size());
            assertEquals(2, output.getRenderResourcesList().size());

            RenderPrototypeDesc.RenderResourceDesc res_desc_1 = output.getRenderResourcesList().get(0);
            assertTrue(res_desc_1.getName().equals("test"));
            assertTrue(res_desc_1.getPath().equals("/test.materialc"));

            RenderPrototypeDesc.RenderResourceDesc res_desc_2 = output.getRenderResourcesList().get(1);
            assertTrue(res_desc_2.getName().equals("test_2"));
            assertTrue(res_desc_2.getPath().equals("/test.materialc"));
        }

        {
            final String srcDuplicateResourceNames =
                "script: \"/test.render_script\"\n" +
                "materials {\n" +
                "   name: \"test\"\n" +
                "   material: \"/test.material\"\n" +
                "}\n" +
                "render_resources {\n" +
                "   name: \"test\"\n" +
                "   path: \"/test.material\"\n" +
                "}\n";

            boolean didFail = false;
            try {
                List<Message> outputs = build("/test3.render", srcDuplicateResourceNames);
            } catch (CompileExceptionError e) {
                didFail = true;
                assertTrue(e.getMessage().contains("contain one or more entries with duplicate names"));
            }
            assertTrue(didFail);
        }
    }
}
