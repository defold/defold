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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertNotNull;

import com.dynamo.bob.fs.IResource;

import org.apache.commons.io.FilenameUtils;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class MeshSetBuilderTest extends AbstractProtoBuilderTest {
    private static final Map<String, String> invalidGLTFFiles = new HashMap<>();

    static {
        invalidGLTFFiles.put("/gltf/accessor_normalized_invalid.gltf", "ACCESSOR_NORMALIZED_INVALID");
        invalidGLTFFiles.put("/gltf/accessor_offset_alignment.gltf", "ACCESSOR_OFFSET_ALIGNMENT");
        invalidGLTFFiles.put("/gltf/buffer_view_too_long.gltf", "BUFFER_VIEW_TOO_LONG");
        invalidGLTFFiles.put("/gltf/node_matrix_and_trs.gltf", "NODE_MATRIX_TRS");
        invalidGLTFFiles.put("/gltf/position_accessor_no_bounds.gltf", "MESH_PRIMITIVE_POSITION_ACCESSOR_WITHOUT_BOUNDS");
        invalidGLTFFiles.put("/gltf/rotation_non_unit.gltf", "ROTATION_NON_UNIT");
        invalidGLTFFiles.put("/gltf/scene_non_root_node.gltf", "SCENE_NON_ROOT_NODE");
        invalidGLTFFiles.put("/gltf/unequal_accessor_count.gltf", "MESH_PRIMITIVE_UNEQUAL_ACCESSOR_COUNT");
        invalidGLTFFiles.put("/gltf/unknown_major_version.gltf", "UNKNOWN_ASSET_MAJOR_VERSION");
    }

    String[] validGLTFFiles = {
            "/gltf/valid.gltf",
            "/gltf/valid.glb",
    };

    @Before
    public void setup() {
        addTestFiles();
    }

    // Return the exception as string if the validation fails
    private String doTest(String glTF) {
        String template =
                "name: \"invalid_gltf_model\"\n" +
                "mesh: \"%s\"\n" +
                "animations: \"%s\"\n" +
                "default_animation: \"invalid\"\n";
        try {
            build("/test_model.model", String.format(template, glTF, glTF));
            return null;
        } catch (Exception e) {
            return e.getMessage();
        }
    }

    @Test
    public void testMeshSetGLTFValid() {
        for (String validGLTFFile : validGLTFFiles) {
            String res = doTest(validGLTFFile);
            assertNull(res);
        }
    }

    @Test
    public void testMeshSetGLTFInvalid() {
        for (Map.Entry<String, String> entry : invalidGLTFFiles.entrySet()) {
            String invalidGLTFFile = entry.getKey();
            String expectedCode = entry.getValue();

            String res = doTest(invalidGLTFFile);
            assertNotNull(res);
            assertTrue(res.contains(expectedCode));
        }
    }

    @Test
    public void testGLTFValidatorValid() throws IOException {
        for (String validGLTFFile : validGLTFFiles) {
            IResource projectRes = getFileSystem().get(validGLTFFile);
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(projectRes.getContent(), FilenameUtils.getExtension(validGLTFFile), true);
            assertTrue(res.result);
        }
    }

    // Same as above, but we need to make sure the flag is working (i.e. we get an ok validation result back).
    @Test
    public void testGLTFValidatorValidNoValidateResources() throws IOException {
        for (String validGLTFFile : validGLTFFiles) {
            IResource projectRes = getFileSystem().get(validGLTFFile);
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(projectRes.getContent(), FilenameUtils.getExtension(validGLTFFile), false);
            assertTrue(res.result);
        }
    }

    @Test
    public void testGLTFValidatorInvalid() throws IOException {
        for (Map.Entry<String, String> entry : invalidGLTFFiles.entrySet()) {
            String invalidGLTFFile = entry.getKey();
            String expectedCode = entry.getValue();

            IResource projectRes = getFileSystem().get(invalidGLTFFile);
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(projectRes.getContent(), FilenameUtils.getExtension(invalidGLTFFile), true);
            assertFalse(res.result);
            assertFalse(res.errors.isEmpty());

            boolean found = false;
            for (GLTFValidator.ValidateError err : res.errors) {
                if (err.code != null && err.code.contains(expectedCode)) {
                    found = true;
                    break;
                }
            }
            assertTrue(found);
        }
    }
}
