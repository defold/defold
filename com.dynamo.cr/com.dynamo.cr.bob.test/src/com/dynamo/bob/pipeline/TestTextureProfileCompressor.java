package com.defold.extension.pipeline.texture;

public class TestTextureProfileCompressor implements ITextureCompressor {
    @Override
    public String getName() {
        return "TestCompressor";
    }

    @Override
    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input) {
        assert(1337 == preset.getOptionInt("option_one"));
        assert(99.0f == preset.getOptionFloat("option_two"));
        assert(preset.getOptionString("option_three").equals("option_three"));

        byte[] out = new byte[4];
        out[0] = (byte) 32;
        out[1] = (byte) 64;
        out[2] = (byte) 128;
        out[3] = (byte) 255;
        return out;
    }
}
