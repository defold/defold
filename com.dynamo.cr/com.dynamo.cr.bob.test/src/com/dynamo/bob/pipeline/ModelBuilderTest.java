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
import static org.junit.Assert.fail;

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.List;

import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ArrayNode;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.GltfMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.gamesys.proto.ModelProto.Model;
import com.dynamo.gamesys.proto.ModelProto.Material;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.RigScene;
import com.google.protobuf.Message;

public class ModelBuilderTest extends AbstractProtoBuilderTest {

    final String GLTF = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA\",\"byteLength\":42}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],\"materials\":[{\"pbrMetallicRoughness\":{}}]}";
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

    @Test(expected=CompileExceptionError.class)
    public void testModelDaeMeshUnsupported() throws Exception {
        addFile("/test_meshset.dae", "unsupported");
        build("/test.model", "mesh: \"/test_meshset.dae\"");
    }

    @Test(expected=CompileExceptionError.class)
    public void testModelDaeSkeletonUnsupported() throws Exception {
        addFile("/test_meshset.gltf", GLTF);
        addFile("/test_skeleton.dae", "unsupported");
        build("/test.model", "mesh: \"/test_meshset.gltf\" skeleton: \"/test_skeleton.dae\"");
    }

    @Test(expected=CompileExceptionError.class)
    public void testModelDaeAnimationUnsupported() throws Exception {
        addFile("/test_meshset.gltf", GLTF);
        addFile("/test_animation.dae", "unsupported");
        build("/test.model", "mesh: \"/test_meshset.gltf\" animations: \"/test_animation.dae\"");
    }

    @Test
    public void testModelGltf() throws Exception {
        addFile("/test_meshset.gltf", GLTF);
        addFile("/test_skeleton.gltf", GLTF);
        addFile("/test_animation.gltf", GLTF);

        StringBuilder srcShader = new StringBuilder();
        srcShader.append("void main() {}\n");

        addFile("/testModelGltf.vp", srcShader.toString());
        addFile("/testModelGltf.fp", srcShader.toString());

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("vertex_program: \"/testModelGltf.vp\"\n");
        src.append("fragment_program: \"/testModelGltf.fp\"\n");

        addFile("/test.material", src.toString());

        src = new StringBuilder();
        src.append(" mesh: \"/test_meshset.gltf\"");
        src.append(" skeleton: \"/test_skeleton.gltf\"");
        src.append(" animations: \"/test_animation.gltf\"");
        src.append(" default_animation: \"test_animation\"");
        src.append(" materials {");
        src.append("   name: \"default\"");
        src.append("   material: \"/test.material\"");
        src.append("}");
        List<Message> outputs = build("/test.model", src.toString());

        Model model = getMessage(outputs, Model.class);
        List<Material> materials = model.getMaterialsList();

        assertEquals("/test.rigscenec", model.getRigScene());
        assertEquals("test_animation", model.getDefaultAnimation());
        assertEquals(-1, model.getMeshIndex());

        assertEquals(1, materials.size());
        assertEquals("default", materials.get(0).getName());
        assertEquals(ResourceUtil.minifyPath("/test.materialc"), materials.get(0).getMaterial());
        assertEquals(ResourceUtil.minifyPath("/test.rigscenec"), model.getRigScene());

        RigScene rigScene = getMessage(outputs, RigScene.class);
        assertEquals("/test_meshset.meshsetc", rigScene.getMeshSet());
        assertEquals(ResourceUtil.minifyPath("/test_skeleton.skeletonc"), rigScene.getSkeleton());
        assertEquals("/test_animation_generated_0.animationsetc", rigScene.getAnimationSet());
    }

    private void addSkinningMaterial(String attribute) {
        addFile("/skinning.glsl", "in vec4 " + attribute + ";\n");
        addFile("/skinned.vp", "#version 140\n#include \"/skinning.glsl\"\nin vec4 position;\nvoid main() { gl_Position = position + " + attribute + "; }\n");
        addFile("/skinned.fp", "#version 140\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n");
        addFile("/skinned.material", "name: \"skinned\" vertex_program: \"/skinned.vp\" fragment_program: \"/skinned.fp\" vertex_space: VERTEX_SPACE_LOCAL");
    }

    // Guards against #10929, including shaders with skin inputs declared in an include and legacy material bindings.
    @Test
    public void testStaticMeshRejectsSkinnedMaterial() throws Exception {
        addFile("/static.gltf", GLTF);
        addSkinningMaterial("bone_weights");
        for (String binding : List.of("materials { name: \"default\" material: \"/skinned.material\" }", "material: \"/skinned.material\"")) {
            try {
                build("/static.model", "mesh: \"/static.gltf\" " + binding);
                fail("Static mesh must reject a skinned material");
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage(), e.getMessage().contains("/skinned.material"));
                assertTrue(e.getMessage(), e.getMessage().contains("bone_weights"));
                assertTrue(e.getMessage(), e.getMessage().contains("/static.gltf"));
                assertTrue(e.getMessage(), e.getMessage().contains("has no skin data"));
            }
        }
    }

    // Verifies either bone input requires skin data, and selected raw meshes are validated even without scene instances.
    @Test
    public void testSelectedStaticMeshRejectsBoneIndices() throws Exception {
        addSkinningMaterial("bone_indices");
        addFile("/selected.gltf", GLTF.replace("\"mesh\":0,", "").replace("\"meshes\":[{", "\"meshes\":[{\"name\":\"Selected\","));
        try {
            build("/selected.model", "mesh: \"/selected.gltf\" mesh_name: \"Selected\" mesh_index: 0 materials { name: \"default\" material: \"/skinned.material\" }");
            fail("Selected static mesh must reject bone indices");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains("bone_indices"));
            assertTrue(e.getMessage(), e.getMessage().contains("has no skin data"));
        }
    }

    // Verifies skin data, rather than animation clips, determines whether a skinned material is compatible.
    @Test
    public void testSkinnedMeshWithoutAnimations() throws Exception {
        addSkinningMaterial("bone_weights");
        try (var stream = getClass().getResourceAsStream("bend2bones.gltf")) {
            addFile("/skinned.gltf", stream.readAllBytes());
        }
        Model model = getMessage(build("/skinned.model", "mesh: \"/skinned.gltf\" skeleton: \"/skinned.gltf\" materials { name: \"Material\" material: \"/skinned.material\" }"), Model.class);
        RigScene rigScene = RigScene.parseFrom(getProject().getResource(model.getRigScene()).output().getContent());
        assertEquals("", rigScene.getAnimationSet());
    }

    // Verifies validation follows material slots and ignores incompatible geometry outside the selected mesh.
    @Test
    public void testMixedStaticAndSkinnedMeshes() throws Exception {
        addSkinningMaterial("bone_weights");
        getProject().mount(new ClassLoaderResourceScanner());
        ObjectMapper mapper = new ObjectMapper();
        ObjectNode scene;
        try (var stream = getClass().getResourceAsStream("bend2bones.gltf")) {
            scene = (ObjectNode) mapper.readTree(stream);
        }
        ArrayNode meshes = (ArrayNode) scene.get("meshes");
        ObjectNode staticMesh = (ObjectNode) mapper.readTree(meshes.get(0).toString());
        staticMesh.put("name", "Static");
        ObjectNode primitive = (ObjectNode) staticMesh.get("primitives").get(0);
        ObjectNode attributes = (ObjectNode) primitive.get("attributes");
        attributes.remove("JOINTS_0");
        attributes.remove("WEIGHTS_0");
        primitive.put("material", 1);
        meshes.add(staticMesh);
        ObjectNode staticMaterial = mapper.createObjectNode();
        staticMaterial.put("name", "StaticMaterial");
        ((ArrayNode) scene.get("materials")).add(staticMaterial);
        ArrayNode nodes = (ArrayNode) scene.get("nodes");
        ((ArrayNode) scene.get("scenes").get(0).get("nodes")).add(nodes.size());
        ObjectNode staticNode = mapper.createObjectNode();
        staticNode.put("mesh", 1);
        staticNode.put("name", "StaticNode");
        nodes.add(staticNode);
        addFile("/mixed.gltf", scene.toString());
        String materials = "materials { name: \"Material\" material: \"/skinned.material\" } " +
                "materials { name: \"StaticMaterial\" material: \"/builtins/materials/model.material\" }";
        build("/mixed.model", "mesh: \"/mixed.gltf\" skeleton: \"/mixed.gltf\" " + materials);
        String incompatibleMaterials = materials.replace("/builtins/materials/model.material", "/skinned.material");
        build("/selected.model", "mesh: \"/mixed.gltf\" skeleton: \"/mixed.gltf\" mesh_name: \"Cube\" mesh_index: 0 " + incompatibleMaterials);
        try {
            build("/mixed.model", "mesh: \"/mixed.gltf\" skeleton: \"/mixed.gltf\" " + incompatibleMaterials);
            fail("Static mesh in a mixed scene must reject a skinned material");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains("StaticMaterial"));
            assertTrue(e.getMessage(), e.getMessage().contains("has no skin data"));
        }
    }

    // Verifies shader edits invalidate model validation even when the compiled material descriptor is unchanged.
    @Test
    public void testShaderEditRevalidatesModel() throws Exception {
        addSkinningMaterial("bone_weights");
        String shader = "#version 140\n#include \"/skinning.glsl\"\nin vec4 position;\nvoid main() { gl_Position = position; }\n";
        addFile("/skinned.vp", shader);
        addFile("/static.gltf", GLTF);
        String modelSource = "mesh: \"/static.gltf\" materials { name: \"default\" material: \"/skinned.material\" }";
        build("/static.model", modelSource);
        addFile("/skinned.vp", shader.replace("gl_Position = position;", "gl_Position = position + bone_weights;"));
        try {
            build("/static.model", modelSource);
            fail("Shader edit must revalidate the model");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains("has no skin data"));
        }
    }

    // Verifies the standalone built-in glTF material and its include graph compile as a model dependency.
    @Test
    public void testModelBuildsWithBuiltinGltfMaterial() throws Exception {
        addFile("/builtin.gltf", GLTF);
        getProject().mount(new ClassLoaderResourceScanner());
        var outputs = build("/builtin.model", "mesh: \"/builtin.gltf\"\nmaterials { name: \"default\" material: \"/builtins/materials/gltf.material\" }\n");
        var model = getMessage(outputs, Model.class);
        assertEquals(ResourceUtil.minifyPath("/builtins/materials/gltf.materialc"), model.getMaterials(0).getMaterial());
        assertTrue(getProject().getResource("/builtins/materials/gltf.material").exists());
    }

    // Verifies generated glTF materials and images build using packaged shaders without an external PBR library.
    @Test
    public void testModelBuildsGltfVirtualMaterialAndImage() throws Exception {
        addImage("/virtual.png", 2, 2);
        String imageData = Base64.getEncoder().encodeToString(getFile("/virtual.png"));
        String virtualGltf = GLTF.replace(
                "\"materials\":[{\"pbrMetallicRoughness\":{}}]",
                "\"images\":[{\"name\":\"VirtualImage\",\"uri\":\"data:image/png;base64," + imageData + "\"}],"
                + "\"textures\":[{\"source\":0}],"
                + "\"materials\":[{\"name\":\"VirtualMaterial\",\"pbrMetallicRoughness\":{"
                + "\"baseColorTexture\":{\"index\":0}}}]");
        addFile("/virtual.gltf", virtualGltf);
        getProject().mount(new ClassLoaderResourceScanner());

        String modelSource =
                "mesh: \"/virtual.gltf\"\n" +
                "materials {\n" +
                "  name: \"VirtualMaterial\"\n" +
                "  material: \"/virtual.gltf/materials/VirtualMaterial_0.material\"\n" +
                "  textures {\n" +
                "    sampler: \"PbrMetallicRoughness_baseColorTexture\"\n" +
                "    texture: \"/virtual.gltf/images/VirtualImage_0.png\"\n" +
                "  }\n" +
                "}\n";

        Model model = getMessage(build("/virtual.model", modelSource), Model.class);
        assertEquals(ResourceUtil.minifyPath("/virtual.gltf/materials/VirtualMaterial_0.materialc"),
                model.getMaterials(0).getMaterial());
        assertEquals("/virtual.gltf/images/VirtualImage_0.texturec",
                model.getMaterials(0).getTextures(0).getTexture());
        assertTrue(getFileSystem().get("build/virtual.gltf/materials/VirtualMaterial_0.materialc").exists());
        assertTrue(getFileSystem().get("build/virtual.gltf/images/VirtualImage_0.texturec").exists());
    }

    // Verifies copied glTF meshes build with the packaged shaders and no project shader stubs.
    @Test
    public void testCopiedMeshesBuildAsModels() throws Exception {
        String primitive = "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}";
        String source = GLTF.replace(primitive, primitive + "," + primitive + ","
                + primitive.replace("{\"primitives", "{\"name\":\"Shared\",\"primitives") + ","
                + primitive.replace("{\"primitives", "{\"name\":\"Shared\",\"primitives"))
                .replace("\"indices\":1", "\"indices\":1,\"material\":0");
        addFile("/meshes.gltf", source);
        getProject().mount(new ClassLoaderResourceScanner());

        var names = List.of("Shared_0", "Shared_1");
        for (int index = 0; index < names.size(); ++index) {
            var mesh = getProject().getResource("/meshes.gltf/meshes/" + names.get(index));
            var outputs = build("/copied" + index + ".model", new String(mesh.getContent(), StandardCharsets.UTF_8));
            var model = getMessage(outputs, Model.class);
            assertEquals(index + 2, model.getMeshIndex());
            assertEquals(ResourceUtil.minifyPath("/meshes.gltf/materials/0.materialc"), model.getMaterials(0).getMaterial());
            var meshSet = MeshSet.parseFrom(getFileSystem().get("build/meshes.meshsetc").getContent());
            assertEquals(2, meshSet.getRawModelsCount());
            assertEquals(1, meshSet.getRawModels(index).getMeshesCount());
        }
    }

    @Test
    public void testDuplicateMeshNameWithDefaultIndex() throws Exception {
        var primitive = "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}";
        var namedPrimitive = primitive.replace("{\"primitives", "{\"name\":\"Shared\",\"primitives");
        addFile("/duplicates.gltf", GLTF.replace(primitive, namedPrimitive + "," + namedPrimitive));

        for (var indexField : List.of("", "mesh_index: 0\n")) {
            var outputs = build("/selected.model",
                    "mesh: \"/duplicates.gltf\"\nmesh_name: \"Shared\"\n" + indexField);
            assertEquals(0, getMessage(outputs, Model.class).getMeshIndex());
        }
    }

    @Test
    public void testWholeSceneWithDefaultIndex() throws Exception {
        addFile("/scene.gltf", GLTF);
        for (var indexField : List.of("", "mesh_index: 0\n")) {
            var outputs = build("/whole.model", "mesh: \"/scene.gltf\"\n" + indexField);
            assertEquals(-1, getMessage(outputs, Model.class).getMeshIndex());
        }
    }

    @Test
    public void testModelGltfMeshSelection() throws Exception {
        String namedGltf = GLTF
                .replace("\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}]", "\"nodes\":[{\"name\":\"Node0\"}]")
                .replace("\"meshes\":[{", "\"meshes\":[{\"name\":\"SelectedMesh\",");
        addFile("/selected_mesh.gltf", namedGltf);

        List<Message> outputs = build("/selected.model",
                "mesh: \"/selected_mesh.gltf\"\n" +
                "mesh_name: \"SelectedMesh\"\n" +
                "mesh_index: 0\n");

        Model model = getMessage(outputs, Model.class);
        assertEquals(0, model.getMeshIndex());

        MeshSet meshSet = getMessage(outputs, MeshSet.class);
        assertEquals(0, meshSet.getModelsCount());
        assertEquals(1, meshSet.getRawModelsCount());
        assertEquals(0, meshSet.getRawModels(0).getMeshIndex());
        assertEquals(0.0f, meshSet.getRawModels(0).getLocal().getTranslation().getX(), 0.0f);
        assertEquals(0.0f, meshSet.getRawModels(0).getLocal().getTranslation().getY(), 0.0f);
        assertEquals(0.0f, meshSet.getRawModels(0).getLocal().getTranslation().getZ(), 0.0f);
    }

    @Test
    public void testInstantiatedSelectedMeshIsNotDuplicated() throws Exception {
        String namedGltf = GLTF.replace("\"meshes\":[{", "\"meshes\":[{\"name\":\"SelectedMesh\",");
        addFile("/selected_mesh.gltf", namedGltf);

        List<Message> outputs = build("/selected.model",
                "mesh: \"/selected_mesh.gltf\"\n" +
                "mesh_name: \"SelectedMesh\"\n" +
                "mesh_index: 0\n");

        Model model = getMessage(outputs, Model.class);
        assertEquals(0, model.getMeshIndex());

        MeshSet meshSet = getMessage(outputs, MeshSet.class);
        assertEquals(1, meshSet.getModelsCount());
        assertEquals(1, meshSet.getRawModelsCount());
        assertEquals(0, meshSet.getModels(0).getMeshIndex());
        assertEquals(0, meshSet.getRawModels(0).getMeshIndex());
        assertEquals(0, meshSet.getRawModels(0).getMeshesCount());
    }

    @Test
    public void testSelectedMeshSceneIsInputOfMeshsetProducer() throws Exception {
        String namedGltf = GLTF.replace("\"meshes\":[{", "\"meshes\":[{\"name\":\"SelectedMesh\",");
        addFile("/selected_mesh.gltf", namedGltf);
        addFile("/selected.model",
                "mesh: \"/selected_mesh.gltf\"\n" +
                "mesh_name: \"SelectedMesh\"\n" +
                "mesh_index: 0\n");

        Task modelTask = getProject().createTask(getProject().getResource("/selected.model"), ModelBuilder.class);
        Task meshsetTask = getProject().createTask(getProject().getResource("/selected_mesh.gltf"), MeshsetBuilder.class);

        assertEquals(1, countInputs(modelTask, "build/selected_mesh.meshsetc"));
        assertEquals(0, countInputs(modelTask, "selected_mesh.gltf"));
        assertEquals(2, modelTask.getInputs().size());
        assertEquals(1, countInputs(meshsetTask, "selected_mesh.gltf"));
    }

    @Test(expected=CompileExceptionError.class)
    public void testModelGltfMissingMeshSelection() throws Exception {
        addFile("/missing_mesh.gltf", GLTF);
        build("/missing.model",
                "mesh: \"/missing_mesh.gltf\"\n" +
                "mesh_name: \"MissingMesh\"\n" +
                "mesh_index: 0\n");
    }

    @Test
    public void testModelTextureWithMixedCaseExtension() throws Exception {
        addFile("/test_meshset.gltf", GLTF);
        addImage("/test.png", 16, 16);
        addFile("/Textures/Test.PnG", getFile("/test.png"));

        String shaderSource = "void main() {}\n";
        addFile("/testModelTexture.vp", shaderSource);
        addFile("/testModelTexture.fp", shaderSource);
        addFile("/test.material",
                "name: \"test_material\"\n" +
                "vertex_program: \"/testModelTexture.vp\"\n" +
                "fragment_program: \"/testModelTexture.fp\"\n");

        String modelSource =
                "mesh: \"/test_meshset.gltf\"\n" +
                "materials {\n" +
                "  name: \"default\"\n" +
                "  material: \"/test.material\"\n" +
                "  textures {\n" +
                "    sampler: \"texture_sampler\"\n" +
                "    texture: \"/Textures/Test.PnG\"\n" +
                "  }\n" +
                "}\n";

        Model model = getMessage(build("/test.model", modelSource), Model.class);
        assertEquals("/Textures/Test.texturec",
                model.getMaterials(0).getTextures(0).getTexture());
    }
}
