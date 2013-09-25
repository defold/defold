package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.List;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile.Playback;
import com.google.protobuf.Message;

public class AtlasBuilderTest extends AbstractProtoBuilderTest {

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    void addImage(String path, int w, int h) throws IOException {
        BufferedImage img = newImage(w, h);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ImageIO.write(img, FilenameUtils.getExtension(path), baos);
        baos.flush();
        addFile(path, baos.toByteArray());
    }

    AtlasBuilder.MappedAnimDesc newAnim(String id, List<String> ids) {
        return new AtlasBuilder.MappedAnimDesc(id, ids, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
    }

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
