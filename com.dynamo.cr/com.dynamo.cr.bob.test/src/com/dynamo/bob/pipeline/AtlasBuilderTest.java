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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.hamcrest.CoreMatchers.is;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;
import java.util.HashSet;

import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.liveupdate.proto.Manifest;
import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.gamesys.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.Message;

import com.dynamo.bob.util.MurmurHash;

public class AtlasBuilderTest extends AbstractProtoBuilderTest {

    @Test
    public void testAtlas() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/test.png\"");
        src.append("}");
        List<Message> outputs = build("/test.atlas", src.toString());
        TextureSet textureSet = getMessage(outputs, TextureSet.class);
        TextureImage textureImage = (TextureImage)outputs.get(1);
        assertNotNull(textureSet);
        assertNotNull(textureImage);
        int expectedSize = (16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1) * 4;
        assertEquals(expectedSize, textureImage.getAlternatives(0).getDataSize());
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

        TextureImage.Image img = textureImage1.getAlternatives(0);
        assertEquals(expectedSize, textureImage1.getAlternatives(0).getDataSize());
    }

    @Test
    public void testAtlasUtilRenameFunction() throws Exception {
        addImage("/hat_nrm.png", 16, 16);
        addImage("/shirt_normal.png", 16, 16);
        addImage("/subfolder1/1.png", 16, 16);
        addImage("/subfolder2/1.png", 16, 16);

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
        src.append("  images: {");
        src.append("    image: \"/subfolder1/1.png\"");
        src.append("  }");
        src.append("}");

        src.append("animations {");
        src.append("id: \"ValidDuplicates\"");
        src.append("  images: {");
        src.append("    image: \"/shirt_normal.png\"");
        src.append("  }");
        src.append("  images: {");
        src.append("    image: \"/subfolder2/1.png\"");
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
        expectedIds.add("ValidDuplicates");

        HashSet<String> ids = new HashSet<>();
        for (TextureSetAnimation anim : textureSet.getAnimationsList()) {
            ids.add(anim.getId());
        }

        assertEquals(expectedIds, ids);
    }

    @Test
    public void testAtlasUtilNoDuplicateFiles1() throws Exception {
        addImage("/1.png", 16, 16);

        // We don't allow duplicate files as single frame animations
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/1.png\"");
        src.append("}");

        src.append("images: {");
        src.append("  image: \"/1.png\"");
        src.append("}");

        boolean caught = false;
        try {
            List<Message> outputs = build("/test.atlas", src.toString());
        } catch (Exception e) {
            caught = true;
        }
        assertTrue(caught);
    }

    @Test
    public void testAtlasUtilNoDuplicateFiles2() throws Exception {
        addImage("/1.png", 16, 16);
        addImage("/2.png", 16, 16);

        // We don't allow duplicate files as single frame animations
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/1.png\"");
        src.append("}");

        src.append("images: {");
        src.append("  image: \"/2.png\"");
        src.append("}");

        // make sure that after the renaming, that we catch this duplicate
        src.append("rename_patterns: \"2=1\"\n");

        boolean caught = false;
        try {
            List<Message> outputs = build("/test.atlas", src.toString());
        } catch (Exception e) {
            caught = true;
        }
        assertTrue(caught);
    }


    @Test
    public void testAtlasRotateImages() throws Exception {
        addImage("/1.png", 10, 12);
        addImage("/2.png", 16, 6);

        // We don't allow duplicate files as single frame animations
        StringBuilder src = new StringBuilder();
        src.append("extrude_borders: 0\n");
        src.append("images: {");
        src.append("  image: \"/1.png\"");
        src.append("}");

        src.append("images: {");
        src.append("  image: \"/2.png\"");
        src.append("}");

        List<Message> outputs = build("/test.atlas", src.toString());

        TextureSet textureSet = (TextureSet)outputs.get(0);

        assertThat(textureSet.getWidth(), is(16));
        assertThat(textureSet.getHeight(), is(16));
        assertThat(textureSet.getAnimationsCount(), is(2));

        TextureSetAnimation anim = textureSet.getAnimations(0);
        assertThat(anim.getId(), is("1"));
        assertThat(anim.getWidth(), is(10));
        assertThat(anim.getHeight(), is(12));

        anim = textureSet.getAnimations(1);
        assertThat(anim.getId(), is("2"));
        assertThat(anim.getWidth(), is(16)); // The unrotated dimensions
        assertThat(anim.getHeight(), is(6));

        SpriteGeometry geom = textureSet.getGeometries(0);
        assertThat(geom.getRotated(), is(false));
        assertThat(geom.getWidth(), is(10));
        assertThat(geom.getHeight(), is(12));

        geom = textureSet.getGeometries(1);
        assertThat(geom.getRotated(), is(true));
        assertThat(geom.getWidth(), is(16)); // The unrotated dimensions
        assertThat(geom.getHeight(), is(6));
    }


    @Test
    public void testAtlasFrameIndices() throws Exception {
        addImage("/a/1.png", 16, 16);
        addImage("/a/2.png", 16, 16);
        addImage("/b/1.png", 16, 16);
        addImage("/b/2.png", 16, 16);

        StringBuilder src = new StringBuilder();
        src.append("animations: {");
        src.append("  id: \"a\"");
        src.append("  images: {");
        src.append("    image: \"/a/1.png\"");
        src.append("  }");
        src.append("  images: {");
        src.append("    image: \"/a/2.png\"");
        src.append("  }");
        src.append("}");

        src.append("animations: {");
        src.append("  id: \"b\"");
        src.append("  images: {");
        src.append("    image: \"/b/1.png\"");
        src.append("  }");
        src.append("  images: {");
        src.append("    image: \"/b/2.png\"");
        src.append("  }");
        src.append("}");


        List<Message> outputs = build("/test.atlas", src.toString());
        TextureSet textureSet = (TextureSet)outputs.get(0);

        assertNotNull(textureSet);

        HashSet<String> expectedIds = new HashSet<>();
        expectedIds.add("a");
        expectedIds.add("b");

        HashSet<String> ids = new HashSet<>();
        for (TextureSetAnimation anim : textureSet.getAnimationsList()) {
            ids.add(anim.getId());
        }

        assertEquals(expectedIds, ids);

        assertThat(textureSet.getWidth(), is(32));
        assertThat(textureSet.getHeight(), is(32));
        assertThat(textureSet.getAnimationsCount(), is(2));

        List<Integer> frameIndices = Arrays.asList(0,1,2,3, 0,1,2,3); // Single frame images + frames in the animations
        assertEquals(frameIndices, textureSet.getFrameIndicesList());

        List<Long> imageNameHashes = new ArrayList<>();
        imageNameHashes.add(MurmurHash.hash64("1"));
        imageNameHashes.add(MurmurHash.hash64("2"));
        imageNameHashes.add(MurmurHash.hash64("1")); // I'm not sure where the tilecount is 4 / MAWE
        imageNameHashes.add(MurmurHash.hash64("2"));

        imageNameHashes.add(MurmurHash.hash64("a/1"));
        imageNameHashes.add(MurmurHash.hash64("a/2"));
        imageNameHashes.add(MurmurHash.hash64("b/1"));
        imageNameHashes.add(MurmurHash.hash64("b/2"));

        assertEquals(imageNameHashes, textureSet.getImageNameHashesList());
    }

    @Test
    public void testHexDigestForAtlas() throws Exception {
        List<String> names = new ArrayList<>();
        for (int i = 1; i <= 10; i++) {
            StringBuilder line = new StringBuilder();
            int min = 1;
            int max = 16;
            String name = "/" + i + ".png";
            addImage(name, (int)(Math.random() * (max - min + 1) + min), (int)(Math.random() * (max - min + 1) + min));
            line.append("images {\n  image: \"");
            line.append(name);
            line.append("\"\n}\n");
            names.add(line.toString());
        }

        Collections.shuffle(names);
        StringBuilder src = new StringBuilder();
        for (String l: names) {
            src.append(l);
        }
        List<Message> outputs1 = build("testHex.atlas", src.toString());
        TextureSet textureSet1 = getMessage(outputs1, TextureSet.class);

        byte[] hash1 = ManifestBuilder.CryptographicOperations.hash(textureSet1.toByteArray(), Manifest.HashAlgorithm.HASH_SHA1);
        String hexDigest1 = ManifestBuilder.CryptographicOperations.hexdigest(hash1);

        clean();

        Collections.shuffle(names);
        StringBuilder src1 = new StringBuilder();
        for (String l: names) {
            src1.append(l);
        }
        List<Message> outputs2 = build("testHex.atlas", src1.toString());
        TextureSet textureSet2 = getMessage(outputs2, TextureSet.class);

        byte[] hash2 = ManifestBuilder.CryptographicOperations.hash(textureSet2.toByteArray(), Manifest.HashAlgorithm.HASH_SHA1);
        String hexDigest2 = ManifestBuilder.CryptographicOperations.hexdigest(hash2);

        assertEquals(hexDigest1, hexDigest2);
    }
}
