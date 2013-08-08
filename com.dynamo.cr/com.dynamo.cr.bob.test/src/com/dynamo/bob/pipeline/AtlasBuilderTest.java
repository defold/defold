package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.lang3.Pair;
import org.junit.Test;

import com.dynamo.bob.atlas.AtlasGenerator;
import com.dynamo.bob.atlas.AtlasGenerator.AnimDesc;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet.Builder;
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

    AnimDesc newAnim(String id, List<String> ids) {
        return new AnimDesc(id, ids, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
    }

    @Test
    public void testAtlasGenerator() throws Exception {
        List<BufferedImage> images =
                Arrays.asList(newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");


        List<AnimDesc> animations = new ArrayList<AtlasGenerator.AnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        Pair<Builder, BufferedImage> pair = AtlasGenerator.generate(images, ids, animations, 0);
        BufferedImage image = pair.right;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        TextureSet textureSet = pair.left.setTexture("").build();

        assertThat(textureSet.getAnimationsCount(), is(4 + 2));
        // NOTE: We currently assume 6 vertices per frame
        // and vertex-format described below
        int vertex_size = 4 * 4; // x, y, z, u, v (3 * float + 2 * short)
        int vertices_per_frame = 6; // two triangles
        assertThat(textureSet.getVertices().size(), is(vertex_size * vertices_per_frame * (4 + 3 + 2)));
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
