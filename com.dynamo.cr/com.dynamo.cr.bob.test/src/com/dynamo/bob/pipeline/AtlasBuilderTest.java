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
