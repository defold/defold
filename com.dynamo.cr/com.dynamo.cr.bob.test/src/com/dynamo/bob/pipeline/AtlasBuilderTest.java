// Copyright 2020-2023 The Defold Foundation
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
import static org.junit.Assert.assertNotNull;

import java.util.List;
import java.util.HashSet;

import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.Message;

public class AtlasBuilderTest extends AbstractProtoBuilderTest {

    @Test
    public void testAtlas() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/test.png\"");
        src.append("}");
        List<Message> outputs = build("/test.atlas", src.toString());
        TextureSet textureSet = (TextureSet)outputs.get(0);
        TextureImage textureImage = (TextureImage)outputs.get(1);
        assertNotNull(textureSet);
        assertNotNull(textureImage);
        int expectedSize = (16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1) * 4;
        assertEquals(expectedSize, textureImage.getAlternatives(0).getData().size());
    }

    @Test
    public void testAtlasPaged() throws Exception {
        addImage("/test1.png", 16, 16);
        addImage("/test2.png", 16, 16);

        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/test1.png\"");
        src.append("}");

        src.append("images: {");
        src.append("  image: \"/test2.png\"");
        src.append("}");

        src.append("max_page_width: 16\n");
        src.append("max_page_height: 16\n");

        List<Message> outputs = build("/test.atlas", src.toString());
        TextureSet textureSet = (TextureSet)outputs.get(0);
        TextureImage textureImage1 = (TextureImage)outputs.get(1);

        assertNotNull(textureSet);
        assertNotNull(textureImage1);

        assertEquals(textureSet.getPageIndices(0), 0);
        assertEquals(textureSet.getPageIndices(1), 1);

        int expectedSize = (16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1) * 4 * 2;
        assertEquals(expectedSize, textureImage1.getAlternatives(0).getData().size());
    }

    @Test
    public void testAtlasUtilRenameFunction() throws Exception {
        addImage("/hat_nrm.png", 16, 16);
        addImage("/shirt_normal.png", 16, 16);

        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/hat_nrm.png\"");
        src.append("}");

        src.append("images: {");
        src.append("  image: \"/shirt_normal.png\"");
        src.append("}");
        src.append("animations {");
        src.append("id: \"Hello\"");
        src.append("  images: {");
        src.append("    image: \"/shirt_normal.png\"");
        src.append("  }");
        src.append("}");

        src.append("max_page_width: 16\n");
        src.append("max_page_height: 16\n");
        src.append("rename_patterns: \"_nrm=,_normal=,hat=cat\"\n");

        List<Message> outputs = build("/test.atlas", src.toString());
        TextureSet textureSet = (TextureSet)outputs.get(0);

        assertNotNull(textureSet);

        HashSet<String> expectedIds = new HashSet<>();
        expectedIds.add("Hello");
        expectedIds.add("cat");
        expectedIds.add("shirt");

        HashSet<String> ids = new HashSet<>();
        for (TextureSetAnimation anim : textureSet.getAnimationsList()) {
            ids.add(anim.getId());
        }

        assertEquals(expectedIds, ids);
    }
}
