// Copyright 2020 The Defold Foundation
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

import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
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
}
