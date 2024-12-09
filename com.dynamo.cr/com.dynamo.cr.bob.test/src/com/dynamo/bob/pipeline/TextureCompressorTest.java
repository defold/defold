// Copyright 2020-2024 The Defold Foundation
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

import com.defold.extension.pipeline.texture.*;
import com.defold.extension.pipeline.texture.TestTextureCompressor;
import org.junit.Test;

import static org.junit.Assert.assertNotNull;

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
        assertNotNull(presetOne);

        assert(presetOne.getOptionInt("test_int") == 7);
        assert(presetOne.getOptionFloat("test_float") == 42.0);
        assert(presetOne.getOptionString("test_string").equals("test_preset_one"));

        TextureCompressorPreset presetTwo = TextureCompression.getPreset("PresetTwo");
        assert(presetTwo.getOptionInt("test_int") == 1337);
        assert(presetTwo.getOptionFloat("test_float") == 999.0);
        assert(presetTwo.getOptionString("test_string").equals("test_preset_two"));
    }

    @Test
    public void testTextureCompressionPresetsGettingUsed() throws Exception {
        TestTextureCompressor testCompressor = new TestTextureCompressor();
        testCompressor.expectedOptionOne = 1234;
        testCompressor.expectedOptionTwo = 8080.0f;
        testCompressor.expectedOptionThree = "test_default_param";

        TextureCompression.registerCompressor(testCompressor);
        ensureBuildProject();
    }
}
