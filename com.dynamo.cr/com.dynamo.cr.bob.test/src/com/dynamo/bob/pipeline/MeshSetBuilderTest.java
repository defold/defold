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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import org.junit.Before;
import org.junit.Test;

public class MeshSetBuilderTest extends AbstractProtoBuilderTest {
    @Before
    public void setup() {
        addTestFiles();
    }

    private boolean doTest(String glTF) {
        String template =
                "name: \"invalid_gltf_model\"\n" +
                "mesh: \"%s\"\n" +
                "animations: \"%s\"\n" +
                "default_animation: \"invalid\"\n";
        try {
            build("/test_model.model", String.format(template, glTF, glTF));
        } catch (Exception e) {
            System.out.println(e.getMessage());
            return false;
        }
        return true;
    }

    @Test
    public void testValidGLTF() {
        boolean ret = doTest("/gltf/valid.gltf");
        assertTrue(ret);
    }

    @Test
    public void testInvalidGLTF() {
        boolean ret = doTest("/gltf/accessor_element_out_of_max_bound.gltf");
        assertFalse(ret);
    }
}
