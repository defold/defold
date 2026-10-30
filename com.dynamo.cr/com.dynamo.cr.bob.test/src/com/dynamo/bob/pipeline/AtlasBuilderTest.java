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
import static org.junit.Assert.fail;
import static org.hamcrest.CoreMatchers.is;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;
import java.util.HashSet;

import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.gamesys.proto.Sprite.SpriteTexture;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.liveupdate.proto.Manifest;
import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.gamesys.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.google.protobuf.Message;

import com.dynamo.bob.util.MurmurHash;

public class AtlasBuilderTest extends AbstractProtoBuilderTest {

    // Vector atlases ignore raster packing, compression and texture profile settings.
    @Test
    public void testSvgAtlasBypassesTexturePacking() throws Exception {
        getProject().setOption("texture-compression", "true");
        getProject().getProjectProperties().putStringValue("graphics", "texture_profiles", "missing.texture_profiles");
        addFile("/image.svg", "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='80'><rect width='100' height='80' fill='red'/></svg>");
        List<Message> outputs = build("/vector.atlas", "images { image: \"/image.svg\" } max_page_width: 8 max_page_height: 8 extrude_borders: 1000");
        assertEquals(1, outputs.size());
        TextureSet atlas = getMessage(outputs, TextureSet.class);
        assertEquals(com.dynamo.gamesys.proto.TextureSetProto.TextureSetMode.TEXTURE_SET_MODE_VECTOR, atlas.getMode());
        assertEquals("", atlas.getTexture());
        assertEquals(100, atlas.getVector().getImages(0).getWidth(), 0);
        assertEquals(0, atlas.getUseGeometries());
    }

    // Custom vector materials support nine-slice; mismatched bindings, texture types and filtering fail.
    @Test
    public void testVectorSpriteCompatibility() throws Exception {
        addFile("/image.svg", "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'><rect width='10' height='10'/></svg>");
        build("/vector.atlas", "images { image: \"/image.svg\" }");
        addFile("/custom.material", "");
        var material = MaterialDesc.newBuilder().setName("custom").setVertexProgram("").setFragmentProgram("").setProgram("/custom.spc");
        var reflection = ShaderDesc.ShaderReflection.newBuilder();
        for (String sampler : List.of("curve_texture", "band_texture", "paint_texture")) {
            material.addSamplers(MaterialDesc.Sampler.newBuilder().setName(sampler)
                    .setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE).setWrapV(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE)
                    .setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST).setFilterMag(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST));
            reflection.addTextures(ShaderDesc.ResourceBinding.newBuilder().setName(sampler).setNameHash(MurmurHash.hash64(sampler))
                    .setType(ShaderDesc.ResourceType.newBuilder().setShaderType(sampler.equals("band_texture")
                            ? ShaderDesc.ShaderDataType.SHADER_TYPE_UTEXTURE2D : ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D)));
        }
        getProject().getResource("/custom.spc").output().setContent(ShaderDesc.newBuilder().setReflection(reflection).build().toByteArray());
        getProject().getResource("/custom.material").changeExt(".materialc").setContent(material.build().toByteArray());
        var builder = new ProtoBuilders.SpriteDescBuilder();
        builder.setProject(getProject());
        var resource = getProject().getResource("/test.sprite");
        var source = SpriteDesc.newBuilder().setDefaultAnimation("image").setMaterial("/custom.material")
                .setSizeMode(SpriteDesc.SizeMode.SIZE_MODE_MANUAL).setSlice9(Vector4.newBuilder().setX(1).setY(2).setZ(3).setW(4))
                .addTextures(SpriteTexture.newBuilder().setSampler("image").setTexture("/vector.atlas"));
        var compiled = builder.transform(null, resource, source.clone());
        assertEquals(1, compiled.getTexturesCount());
        assertEquals("curve_texture", compiled.getTextures(0).getSampler());
        assertEquals("/vector.a.texturesetc", compiled.getTextures(0).getTexture());
        assertEquals(source.getSlice9(), compiled.getSlice9());
        assertEquals(SpriteDesc.SizeMode.SIZE_MODE_MANUAL, compiled.getSizeMode());
        List<SpriteDesc.Builder> invalid = List.of(
                source.clone().addTextures(source.getTextures(0)),
                source.clone().setMaterial(""));
        String[] errors = {"one atlas image", "atlas/material mismatch"};
        for (int i = 0; i < invalid.size(); ++i) {
            try {
                builder.transform(null, resource, invalid.get(i));
                fail("Expected " + errors[i]);
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage(), e.getMessage().contains(errors[i]));
            }
        }
        for (int i = 0; i < 2; ++i) {
            if (i == 0) {
                material.getSamplersBuilder(1).setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR);
            } else {
                material.getSamplersBuilder(1).setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST);
                reflection.getTexturesBuilder(1).setType(ShaderDesc.ResourceType.newBuilder().setShaderType(ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D));
                getProject().getResource("/custom.spc").output().setContent(ShaderDesc.newBuilder().setReflection(reflection).build().toByteArray());
            }
            getProject().getResource("/custom.material").changeExt(".materialc").setContent(material.build().toByteArray());
            try {
                builder.transform(null, resource, source.clone());
                fail("Expected numeric sampler contract error");
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage(), e.getMessage().contains("band_texture"));
                assertTrue(e.getMessage(), e.getMessage().contains(i == 0 ? "nearest filtering" : "unsigned 2D texture"));
            }
        }
    }

    @Test
    public void testAtlas() throws Exception {
        addImage("/test.png", 16, 16);
        String src = "images: {" +
                "  image: \"/test.png\"" +
                "}";
        List<Message> outputs = build("/test.atlas", src);
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

        String src = "images: {" +
                "  image: \"/test1.png\"" +
                "}" +
                "images: {" +
                "  image: \"/test2.png\"" +
                "}" +
                "max_page_width: 16\n" +
                "max_page_height: 16\n";

        List<Message> outputs = build("/test.atlas", src);
        TextureSet textureSet = (TextureSet)outputs.get(0);
        TextureImage textureImage1 = (TextureImage)outputs.get(1);

        assertNotNull(textureSet);
        assertNotNull(textureImage1);

        assertEquals(0, textureSet.getPageIndices(0));
        assertEquals(1, textureSet.getPageIndices(1));

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

        String src = "images: {" +
                "  image: \"/hat_nrm.png\"" +
                "}" +
                "images: {" +
                "  image: \"/shirt_normal.png\"" +
                "}" +
                "animations {" +
                "id: \"Hello\"" +
                "  images: {" +
                "    image: \"/shirt_normal.png\"" +
                "  }" +
                "  images: {" +
                "    image: \"/subfolder1/1.png\"" +
                "  }" +
                "}" +
                "animations {" +
                "id: \"ValidDuplicates\"" +
                "  images: {" +
                "    image: \"/shirt_normal.png\"" +
                "  }" +
                "  images: {" +
                "    image: \"/subfolder2/1.png\"" +
                "  }" +
                "}" +
                "max_page_width: 16\n" +
                "max_page_height: 16\n" +
                "rename_patterns: \"_nrm=,_normal=,hat=cat\"\n";

        List<Message> outputs = build("/test.atlas", src);
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

        String src = "extrude_borders: 0\n" +
                "images: {" +
                "  image: \"/1.png\"" +
                "}" +
                "images: {" +
                "  image: \"/2.png\"" +
                "}";

        List<Message> outputs = build("/test.atlas", src);

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

        String src = "animations: {" +
                "  id: \"a\"" +
                "  images: {" +
                "    image: \"/a/1.png\"" +
                "  }" +
                "  images: {" +
                "    image: \"/a/2.png\"" +
                "  }" +
                "}" +
                "animations: {" +
                "  id: \"b\"" +
                "  images: {" +
                "    image: \"/b/1.png\"" +
                "  }" +
                "  images: {" +
                "    image: \"/b/2.png\"" +
                "  }" +
                "}";


        List<Message> outputs = build("/test.atlas", src);
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

    // https://github.com/defold/defold/issues/13019
    @Test
    public void testAtlasImagePivotVariants() throws Exception {
        addImage("/test_image.png", 16, 16);

        String src = "margin: 0\n" +
                "extrude_borders: 0\n" +
                "images: {" +
                "  image: \"/test_image.png\"" +
                "}" +
                "animations: {" +
                "  id: \"anim\"" +
                "  images: {" +
                "    image: \"/test_image.png\"" +
                "    pivot_x: 0.0" +
                "  }" +
                "}";

        List<Message> outputs = build("/test.atlas", src);
        TextureSet textureSet = (TextureSet)outputs.get(0);
        assertNotNull(textureSet);

        // One packed rect, so the atlas is the size of a single image.
        assertThat(textureSet.getWidth(), is(16));
        assertThat(textureSet.getHeight(), is(16));

        // But a geometry each. SpriteGeometry stores the pivot relative to the image center with
        // +Y up, so an authored 0.5 becomes 0.0 and an authored 0.0 becomes -0.5.
        assertThat(textureSet.getGeometriesCount(), is(2));
        assertEquals(-0.5f, textureSet.getGeometries(0).getPivotX(), 0.0001f);
        assertEquals(0.0f, textureSet.getGeometries(1).getPivotX(), 0.0001f);

        // Tile quads first, then "anim" and the standalone image, pointing at a geometry each.
        assertEquals(Arrays.asList(0, 1, 0, 1), textureSet.getFrameIndicesList());
    }

    // https://github.com/defold/defold/issues/7403
    @Test
    public void testAtlasImageTrimModeVariants() throws Exception {
        addImage("/test_image.png", 16, 16);

        String src = "margin: 0\n" +
                "extrude_borders: 0\n" +
                "images: {" +
                "  image: \"/test_image.png\"" +
                "}" +
                "animations: {" +
                "  id: \"anim\"" +
                "  images: {" +
                "    image: \"/test_image.png\"" +
                "    sprite_trim_mode: SPRITE_TRIM_MODE_4" +
                "  }" +
                "}";

        List<Message> outputs = build("/test.atlas", src);
        TextureSet textureSet = (TextureSet)outputs.get(0);
        assertNotNull(textureSet);

        // One packed rect, but a geometry each.
        assertThat(textureSet.getWidth(), is(16));
        assertThat(textureSet.getHeight(), is(16));
        assertThat(textureSet.getGeometriesCount(), is(2));
        assertThat(textureSet.getGeometries(0).getTrimMode(), is(SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        assertThat(textureSet.getGeometries(1).getTrimMode(), is(SpriteTrimmingMode.SPRITE_TRIM_MODE_4));
        assertThat(textureSet.getUseGeometries(), is(1));

        // Images sort by path, then pivot, then trim mode number, so the untrimmed entry is first.
        assertEquals(Arrays.asList(0, 1, 1, 0), textureSet.getFrameIndicesList());
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
