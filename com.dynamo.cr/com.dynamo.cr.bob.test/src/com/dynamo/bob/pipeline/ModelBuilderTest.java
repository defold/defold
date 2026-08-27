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

import java.util.Base64;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

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
        getFileSystem().addMountPoint(new GltfMountPoint(getFileSystem()));

        String shaderSource = "void main() {}\n";
        addFile("/defold-pbr/shaders/pbr.vp", shaderSource);
        addFile("/defold-pbr/shaders/pbr.fp", shaderSource);

        String modelSource =
                "mesh: \"/virtual.gltf\"\n" +
                "materials {\n" +
                "  name: \"VirtualMaterial\"\n" +
                "  material: \"/virtual.gltf/materials/0.material\"\n" +
                "  textures {\n" +
                "    sampler: \"PbrMetallicRoughness_baseColorTexture\"\n" +
                "    texture: \"/virtual.gltf/images/0.png\"\n" +
                "  }\n" +
                "}\n";

        Model model = getMessage(build("/virtual.model", modelSource), Model.class);
        assertEquals(ResourceUtil.minifyPath("/virtual.gltf/materials/0.materialc"),
                model.getMaterials(0).getMaterial());
        assertEquals("/virtual.gltf/images/0.texturec",
                model.getMaterials(0).getTextures(0).getTexture());
        assertTrue(getFileSystem().get("build/virtual.gltf/materials/0.materialc").exists());
        assertTrue(getFileSystem().get("build/virtual.gltf/images/0.texturec").exists());
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
