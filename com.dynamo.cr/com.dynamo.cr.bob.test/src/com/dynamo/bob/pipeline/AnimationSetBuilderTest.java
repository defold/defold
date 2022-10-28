// Copyright 2020-2022 The Defold Foundation
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.RigAnimation;
import com.google.protobuf.Message;

public class AnimationSetBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private static RigAnimation getAnim(Map<Long, RigAnimation> animations, String id) {
        return animations.get(MurmurHash.hash64(id));
    }

    private static Map<Long, RigAnimation> getAnims(AnimationSet animSet) {
        Map<Long, RigAnimation> anims = new HashMap<Long, RigAnimation>();
        for (RigAnimation anim : animSet.getAnimationsList()) {
            anims.put(anim.getId(), anim);
        }
        return anims;
    }

    private void addTestFile(String sourcePath, String resourcFilePath) {
        InputStream is = getClass().getResourceAsStream(sourcePath);
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        try {
            IOUtils.copy(is, os);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            IOUtils.closeQuietly(is);
        }
        addFile(resourcFilePath, os.toByteArray());
    }

    @Test
    public void testAnimationSet() throws Exception {
        addTestFile("testanim.dae", "testanim.dae");
        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/testanim.dae\" }");
        src.append("skeleton: \"/testanim.dae\"");
        List<Message> outputs = build("/test.animationset", src.toString());

        Map<Long, RigAnimation> anims = getAnims((AnimationSet)outputs.get(0));
        assertEquals(1,anims.size());
        assertTrue(null != getAnim(anims, "testanim"));
    }

    @Test
    public void testAnimationSetHierarchy() throws Exception {
        addTestFile("testanim.dae", "testanim1.dae");
        addTestFile("testanim.dae", "testanim2.dae");
        addTestFile("testanim.dae", "testanim3.dae");
        addTestFile("testanim.dae", "1/testanim3.dae");

        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/1/testanim3.dae\" }");
        src.append("skeleton: \"/testanim1.dae\"");
        addFile("testset2.animationset", src.toString());
        src = new StringBuilder();
        src.append("animations { animation : \"/testanim3.dae\" }");
        src.append("animations { animation : \"/testset2.animationset\" }");
        src.append("skeleton: \"/testanim1.dae\"");
        addFile("testset1.animationset", src.toString());
        src = new StringBuilder();
        src.append("animations { animation : \"/testanim1.dae\" }");
        src.append("animations { animation : \"/testanim2.dae\" }");
        src.append("animations { animation : \"/testset1.animationset\" }");
        src.append("skeleton: \"/testanim1.dae\"");
        List<Message> outputs = build("/test.animationset", src.toString());

        Map<Long, RigAnimation> anims = getAnims((AnimationSet)outputs.get(0));
        assertEquals(4,anims.size());
        assertTrue(null != getAnim(anims, "testanim1"));
        assertTrue(null != getAnim(anims, "testanim2"));
        assertTrue(null != getAnim(anims, "testset1/testanim3"));
        assertTrue(null != getAnim(anims, "testset2/testanim3"));
    }

    @Test(expected=CompileExceptionError.class)
    public void testAnimationSetMultipleAnimationReference() throws Exception {
        addTestFile("testanim.dae", "testanim.dae");
        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/testanim.dae\" }");
        src.append("animations { animation : \"/testanim.dae\" }");
        build("/test.animationset", src.toString());
    }

    @Test(expected=CompileExceptionError.class)
    public void testAnimationSetMultipleAnimationId() throws Exception {
        addTestFile("testanim.dae", "1/testanim.dae");
        addTestFile("testanim.dae", "2/testanim.dae");
        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/1/testanim.dae\" }");
        src.append("animations { animation : \"/2/testanim.dae\" }");
        build("/test.animationset", src.toString());
    }

    @Test(expected=CompileExceptionError.class)
    public void testAnimationSetMultipleAnimationReferenceHierarchy() throws Exception {
        addTestFile("testanim.dae", "testanim.dae");
        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/testanim.dae\" }");
        addFile("testset.animationset", src.toString());
        src = new StringBuilder();
        src.append("animations { animation : \"/testanim.dae\" }");
        src.append("animations { animation : \"/testset.animationset\" }");
        build("/test.animationset", src.toString());
    }

    @Test(expected=CompileExceptionError.class)
    public void testAnimationSetCircularReference() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("animations { animation : \"/test.animationset\" }");
        build("/test.animationset", src.toString());
    }

}
