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

package com.defold.extension.pipeline.texture;

import com.dynamo.graphics.proto.Graphics;

public class TestTextureCompressor implements ITextureCompressor {

    public static int expectedOptionOne;
    public static float expectedOptionTwo;
    public static String expectedOptionThree;
    public static byte[] expectedBytes = new byte[4];

    public static boolean didRun = false;

    @Override
    public String getName() {
        return "TestCompressor";
    }

    @Override
    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input) {
        assert(expectedOptionOne == preset.getOptionInt("test_int"));
        assert(expectedOptionTwo == preset.getOptionFloat("test_float"));
        assert(expectedOptionThree.equals(preset.getOptionString("test_string")));
        didRun = true;
        return expectedBytes;
    }

    @Override
    public boolean supportsTextureFormat(Graphics.TextureImage.TextureFormat format) {
        return true;
    }

    @Override
    public boolean supportsTextureCompressorPreset(TextureCompressorPreset preset) {
        Integer test_int = preset.getOptionInt("test_int");
        Float test_float = preset.getOptionFloat("test_float");
        String test_string = preset.getOptionString("test_string");
        return test_int != null && test_float != null && test_string != null;
    }
}
