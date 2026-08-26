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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
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

    private static int countInputs(Task task, String path) {
        int count = 0;
        for (IResource input : task.getInputs()) {
            if (path.equals(input.getPath())) {
                ++count;
            }
        }
        return count;
    }

    private static String makeExternalBufferGltf() {
        return "{" +
                "\"asset\":{\"version\":\"2.0\"}," +
                "\"buffers\":[" +
                "{\"uri\":\"mesh.bin\",\"byteLength\":1}," +
                "{\"uri\":\"mesh.bin\",\"byteLength\":1}," +
                "{\"uri\":\"data:application/octet-stream;base64,AA==\",\"byteLength\":1}" +
                "]}";
    }

    private static byte[] makeGlb(String json) {
        byte[] jsonBytes = json.getBytes(StandardCharsets.UTF_8);
        int paddedJsonLength = (jsonBytes.length + 3) & ~3;
        int totalLength = 12 + 8 + paddedJsonLength;
        ByteBuffer glb = ByteBuffer.allocate(totalLength).order(ByteOrder.LITTLE_ENDIAN);
        glb.putInt(0x46546c67);
        glb.putInt(2);
        glb.putInt(totalLength);
        glb.putInt(paddedJsonLength);
        glb.putInt(0x4e4f534a);
        glb.put(jsonBytes);
        while (glb.position() < totalLength) {
            glb.put((byte) ' ');
        }
        return glb.array();
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
            assertTrue(res.result());
        }
    }

    // Same as above, but we need to make sure the flag is working (i.e. we get an ok validation result back).
    @Test
    public void testGLTFValidatorValidNoValidateResources() throws IOException {
        for (String validGLTFFile : validGLTFFiles) {
            IResource projectRes = getFileSystem().get(validGLTFFile);
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(projectRes.getContent(), FilenameUtils.getExtension(validGLTFFile), false);
            assertTrue(res.result());
        }
    }

    @Test
    public void testGLTFValidatorValidExternalResources() throws IOException {
        Path tmpDir = Files.createTempDirectory("gltf_external_resources");
        try {
            IResource gltfRes = getFileSystem().get("/gltf/valid_external_resources.gltf");
            IResource binRes = getFileSystem().get("/gltf/valid_external_resources.bin");
            Path gltfPath = tmpDir.resolve("valid_external_resources.gltf");
            Path binPath = tmpDir.resolve("valid_external_resources.bin");
            Files.write(gltfPath, gltfRes.getContent());
            Files.write(binPath, binRes.getContent());
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(gltfPath.toAbsolutePath().toString(), true);
            assertTrue(res.result());
        } finally {
            FileUtils.deleteDirectory(tmpDir.toFile());
        }
    }

    @Test
    public void testGLTFValidatorInvalid() throws IOException {
        for (Map.Entry<String, String> entry : invalidGLTFFiles.entrySet()) {
            String invalidGLTFFile = entry.getKey();
            String expectedCode = entry.getValue();

            IResource projectRes = getFileSystem().get(invalidGLTFFile);
            GLTFValidator.ValidateResult res = GLTFValidator.validateGltf(projectRes.getContent(), FilenameUtils.getExtension(invalidGLTFFile), true);
            assertFalse(res.result());
            assertFalse(res.errors().isEmpty());

            boolean found = false;
            for (GLTFValidator.ValidateError err : res.errors()) {
                if (err.code() != null && err.code().contains(expectedCode)) {
                    found = true;
                    break;
                }
            }
            assertTrue(found);
        }
    }

    @Test
    public void testExternalBuffersAreInputsOfMeshsetProducer() throws Exception {
        addFile("/mesh.bin", new byte[] { 0 });
        addFile("/mesh.gltf", makeExternalBufferGltf());

        Task meshsetTask = getProject().createTask(getProject().getResource("/mesh.gltf"), MeshsetBuilder.class);

        assertEquals(1, countInputs(meshsetTask, "mesh.gltf"));
        assertEquals(1, countInputs(meshsetTask, "mesh.bin"));
        assertEquals(2, meshsetTask.getInputs().size());
    }

    @Test
    public void testExternalBuffersResolveRelativeToSceneResource() throws Exception {
        byte[] buffer = new byte[] { 1, 2, 3 };
        addFile("/buffers/mesh.bin", buffer);
        addFile("/models/mesh.gltf", "{" +
                "\"asset\":{\"version\":\"2.0\"}," +
                "\"buffers\":[{\"uri\":\"../buffers/mesh.bin\",\"byteLength\":3}]}");

        IResource sceneResource = getProject().getResource("/models/mesh.gltf");
        Task meshsetTask = getProject().createTask(sceneResource, MeshsetBuilder.class);

        assertEquals(1, countInputs(meshsetTask, "buffers/mesh.bin"));
        assertArrayEquals(buffer, new GltfResourceUtil.ResourceDataResolver(sceneResource)
                .getData("/ignored/host/path", "../buffers/mesh.bin"));
    }

    @Test
    public void testMorphOutputsAreDiscoveredWithImporterFallback() throws Exception {
        String gltf = "{" +
                "\"asset\":{\"version\":\"2.0\"}," +
                "\"accessors\":[" +
                "{\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"," +
                "\"min\":[0,0,0],\"max\":[0,0,0]}," +
                "{\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}]," +
                "\"meshes\":[{\"name\":\"MorphMesh\",\"primitives\":[{" +
                "\"attributes\":{\"POSITION\":0},\"targets\":[{\"POSITION\":1}]}]}]," +
                "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
        addFile("/morph.gltf", gltf);

        Task meshsetTask = getProject().createTask(getProject().getResource("/morph.gltf"), MeshsetBuilder.class);

        assertEquals(5, meshsetTask.getOutputs().size());
        assertEquals("build/morph_morph_0.texturec", meshsetTask.output(4).getPath());
    }

    @Test
    public void testGlbMetadataScanFindsExternalBuffersAndMorphTargets() throws Exception {
        String json = "{" +
                "\"asset\":{\"version\":\"2.0\"}," +
                "\"buffers\":[{\"uri\":\"mesh.bin\",\"byteLength\":1}]," +
                "\"meshes\":[{\"name\":\"Named\",\"primitives\":[{\"targets\":[{}]}]}]}";

        GltfResourceUtil.Metadata metadata = GltfResourceUtil.scan(makeGlb(json), "/mesh.glb");

        assertEquals(1, metadata.externalBufferUris().size());
        assertEquals("mesh.bin", metadata.externalBufferUris().get(0));
        assertTrue(metadata.hasMorphTargets());
    }

    @Test
    public void testModelAndCollisionObjectShareMeshsetProducer() throws Exception {
        addFile("/mesh.bin", new byte[] { 0 });
        addFile("/mesh.gltf", makeExternalBufferGltf());
        addFile("/mesh.model",
                "mesh: \"/mesh.gltf\"\n" +
                "mesh_name: \"Ground\"\n" +
                "mesh_index: 0\n");
        addFile("/mesh.collisionobject",
                "type: COLLISION_OBJECT_TYPE_STATIC\n" +
                "mass: 0\n" +
                "embedded_collision_shape {\n" +
                "  shapes {\n" +
                "    shape_type: TYPE_MESH\n" +
                "    id: \"mesh\"\n" +
                "    position { x: 0 y: 0 z: 0 }\n" +
                "    rotation { x: 0 y: 0 z: 0 w: 1 }\n" +
                "    index: 0\n" +
                "    count: 0\n" +
                "    mesh_scene: \"/mesh.gltf\"\n" +
                "    mesh_name: \"Ground\"\n" +
                "    mesh_index: 0\n" +
                "  }\n" +
                "}\n");

        Task modelTask = getProject().createTask(getProject().getResource("/mesh.model"), ModelBuilder.class);
        Task collisionObjectTask = getProject().createTask(getProject().getResource("/mesh.collisionobject"), CollisionObjectBuilder.class);

        assertEquals(1, countInputs(modelTask, "build/mesh.meshsetc"));
        assertEquals(1, countInputs(collisionObjectTask, "build/mesh.meshsetc"));

        int meshsetTaskCount = 0;
        for (Task task : getProject().getTasks()) {
            if (task.getBuilder() instanceof MeshsetBuilder && "mesh.gltf".equals(task.firstInput().getPath())) {
                ++meshsetTaskCount;
            }
        }
        assertEquals(1, meshsetTaskCount);
    }
}
