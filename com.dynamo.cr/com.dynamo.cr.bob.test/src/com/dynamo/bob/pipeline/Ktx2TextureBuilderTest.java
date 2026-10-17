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

import static org.junit.Assert.*;

import java.io.ByteArrayOutputStream;
import org.junit.Test;
import com.defold.extension.pipeline.texture.TextureCompression;
import com.defold.extension.pipeline.texture.TextureCompressorBasisU;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.CompressionType;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;

public class Ktx2TextureBuilderTest extends AbstractProtoBuilderTest {
    @Test
    public void standaloneAndVirtualTexturesCompileToTexturec() throws Exception {
        byte[] source = Ktx2TextureGeneratorTest.fixture("uastc-zstd");
        ByteArrayOutputStream expected = new ByteArrayOutputStream();
        TextureUtil.writeGenerateResultToOutputStream(TextureGenerator.generate(source, null, false), expected);
        for (String path : new String[] {"/source.ktx2", "/models/robot.glb/images/0.ktx2"}) {
            addFile(path, source);
            Task task = getProject().createTask(getProject().getResource(path));
            assertNotNull(task);
            assertTrue(task.getBuilder() instanceof TextureBuilder);
            task.getBuilder().build(task);
            assertTrue(task.output(0).getPath().endsWith(".texturec"));
            assertArrayEquals(expected.toByteArray(), task.output(0).getContent());
            assertEquals(path.substring(0, path.length() - 5) + ".texturec", ProtoBuilders.replaceTextureName(path));
        }
    }

    // Standalone and virtual paths must apply their matched profile and compression setting.
    @Test
    public void textureProfilePathRulesProcessStandaloneAndVirtualKtx2() throws Exception {
        TextureCompression.registerCompressor(new TextureCompressorBasisU());
        getProject().setOption("texture-compression", "true");
        getProject().getProjectProperties().putStringValue("graphics", "texture_profiles", "test.texture_profiles");
        addFile("/test.texture_profiles",
                "path_settings { path: \"/textures/*.ktx2\" profile: \"Compressed\" }\n" +
                "path_settings { path: \"/models/**/images/*.ktx2\" profile: \"Uncompressed\" }\n" +
                "profiles { name: \"Compressed\" platforms { os: OS_ID_GENERIC mipmaps: false max_texture_size: 4 " +
                "premultiply_alpha: false " +
                "formats { format: TEXTURE_FORMAT_RGBA compressor: \"BasisU\" compressor_preset: \"BASISU_LOW\" } } }\n" +
                "profiles { name: \"Uncompressed\" platforms { os: OS_ID_GENERIC mipmaps: true max_texture_size: 2 " +
                "premultiply_alpha: false " +
                "formats { format: TEXTURE_FORMAT_RGB_16BPP compressor: \"Uncompressed\" compressor_preset: \"UNCOMPRESSED\" } } }");
        Task profiles = getProject().createTask(getProject().getResource("/test.texture_profiles"));
        profiles.getBuilder().build(profiles);

        String[] paths = {"/textures/source.ktx2", "/models/robot.glb/images/0.ktx2"};
        TextureImage.Image[] alternatives = new TextureImage.Image[paths.length];
        Task[] textures = new Task[paths.length];
        for (int i = 0; i < paths.length; ++i) {
            addFile(paths[i], Ktx2TextureGeneratorTest.fixture("uastc-zstd"));
            Task texture = getProject().createTask(getProject().getResource(paths[i]));
            assertEquals(profiles.output(0), texture.lastInput());
            texture.getBuilder().build(texture);
            TextureImage result = TextureUtil.textureResourceBytesToTextureImage(texture.output(0).getContent());
            assertEquals(1, result.getAlternativesCount());
            alternatives[i] = result.getAlternatives(0);
            textures[i] = texture;
        }

        assertEquals(4, alternatives[0].getWidth());
        assertEquals(2, alternatives[0].getHeight());
        assertEquals(1, alternatives[0].getMipMapOffsetCount());
        assertEquals(CompressionType.COMPRESSION_TYPE_BASIS_UASTC, alternatives[0].getCompressionType());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, alternatives[0].getFormat());

        assertEquals(2, alternatives[1].getWidth());
        assertEquals(1, alternatives[1].getHeight());
        assertEquals(2, alternatives[1].getMipMapOffsetCount());
        assertEquals(CompressionType.COMPRESSION_TYPE_DEFAULT, alternatives[1].getCompressionType());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_16BPP, alternatives[1].getFormat());

        getProject().setOption("texture-compression", "false");
        textures[0].getBuilder().build(textures[0]);
        TextureImage.Image uncompressed = TextureUtil.textureResourceBytesToTextureImage(textures[0].output(0).getContent()).getAlternatives(0);
        assertEquals(CompressionType.COMPRESSION_TYPE_DEFAULT, uncompressed.getCompressionType());
        assertEquals(4, uncompressed.getWidth());
        assertEquals(1, uncompressed.getMipMapOffsetCount());
    }

    @Test
    public void malformedImageErrorNamesTheResource() throws Exception {
        byte[] source = Ktx2TextureGeneratorTest.fixture("uastc");
        source[80] = 0; source[81] = 0;
        addFile("/broken.ktx2", source);
        Task task = getProject().createTask(getProject().getResource("/broken.ktx2"));
        try {
            task.getBuilder().build(task);
            fail("Malformed KTX2 should fail the resource build");
        } catch (CompileExceptionError expected) {
            assertEquals(task.input(0), expected.getResource());
            assertTrue(expected.getMessage().contains("KTX2"));
        }
    }
}
