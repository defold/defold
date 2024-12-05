package com.dynamo.bob.pipeline;

import com.defold.extension.pipeline.texture.ITextureCompressor;
import com.defold.extension.pipeline.texture.TextureCompression;
import com.defold.extension.pipeline.texture.TextureCompressorParams;
import com.defold.extension.pipeline.texture.TextureCompressorPreset;
import org.junit.Test;

import java.io.IOException;

import static org.junit.Assert.assertEquals;

public class TextureCompressorTest extends AbstractProtoBuilderTest {

    private void ensureBuildProject() throws Exception {
        // We need to build some dummy data
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/test.png\"");
        src.append("}");
        build("/test.atlas", src.toString());
    }

    @Test
    public void testTextureCompressionPresets() throws Exception {
        ensureBuildProject();

        TextureCompressorPreset presetOne = TextureCompression.getPreset("PresetOne");
        assert(presetOne.getOptionInt("test_int") == 7);
        assert(presetOne.getOptionFloat("test_float") == 42.0);
        assert(presetOne.getOptionString("test_string").equals("test_preset_one"));

        TextureCompressorPreset presetTwo = TextureCompression.getPreset("PresetTwo");
        assert(presetTwo.getOptionInt("test_int") == 1337);
        assert(presetTwo.getOptionFloat("test_float") == 999.0);
        assert(presetTwo.getOptionString("test_string").equals("test_preset_two"));
    }

    private class TestTextureCompressor implements ITextureCompressor {
        // For now we are hijacking the "Default" compressor for this test.
        @Override
        public String getName() {
            return "Default";
        }

        @Override
        public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input) {

            assert(preset.getOptionInt("test_int") == 1234);
            assert(preset.getOptionFloat("test_float") == 8080.0);
            assert(preset.getOptionString("test_string").equals("test_default_param"));

            byte[] bytes = new byte[4];
            bytes[0] = 1;
            bytes[1] = 2;
            bytes[2] = 3;
            bytes[3] = 4;
            return bytes;
        }
    }

    @Test
    public void testTextureCompressionPresetsGettingUsed() throws Exception {
        TestTextureCompressor testCompressor = new TestTextureCompressor();
        TextureCompression.registerCompressor(testCompressor);
        ensureBuildProject();
    }
}
