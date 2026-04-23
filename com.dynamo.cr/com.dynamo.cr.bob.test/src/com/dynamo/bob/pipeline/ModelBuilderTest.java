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
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.gamesys.proto.ModelProto.Model;
import com.dynamo.gamesys.proto.ModelProto.Material;
import com.dynamo.rig.proto.Rig.RigScene;
import com.google.protobuf.Message;

public class ModelBuilderTest extends AbstractProtoBuilderTest {

    final String GLTF = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAgD8AAAAAAAAAAADwPwAAAAAAAPA/AAAAAAAAgD8AIAAAAAAAQAAAAAAAAEAAAAAAAAA=\",\"byteLength\":42}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],\"materials\":[{\"pbrMetallicRoughness\":{}}]}";
    @Before
    public void setup() {
        addTestFiles();
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

        assertEquals(1, materials.size());
        assertEquals("default", materials.get(0).getName());
        assertEquals(ResourceUtil.minifyPath("/test.materialc"), materials.get(0).getMaterial());
        assertEquals(ResourceUtil.minifyPath("/test.rigscenec"), model.getRigScene());

        RigScene rigScene = getMessage(outputs, RigScene.class);
        assertEquals("/test_meshset.meshsetc", rigScene.getMeshSet());
        assertEquals(ResourceUtil.minifyPath("/test_skeleton.skeletonc"), rigScene.getSkeleton());
        assertEquals("/test_animation_generated_0.animationsetc", rigScene.getAnimationSet());
    }
}
