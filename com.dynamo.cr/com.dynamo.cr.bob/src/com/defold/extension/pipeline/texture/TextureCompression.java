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


package com.defold.extension.pipeline.texture;


import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.TextureGenerator;

import java.util.HashMap;

/**
 * Interface for compressing a texture using settings
 * We will scan for public non-abstract classes that implement this interface inside the com.defold.extension.pipeline
 * package and instantiate them to transform Lua source code. Implementors must provide a no-argument constructor.
 */
public class TextureCompression {

    private static HashMap<String, ITextureCompressor> compressors = new HashMap<>();
    private static HashMap<String, TextureCompressorPreset> presets = new HashMap<>();

    private static Logger logger = Logger.getLogger(TextureGenerator.class.getName());

    public static void registerCompressor(ITextureCompressor compressor) {
        compressors.put(compressor.getName(), compressor);

        logger.info(String.format("Registered texture compressor: '%s'\n", compressor.getName()));
    }

    public static ITextureCompressor getCompressor(String name) {
        ITextureCompressor compressor = compressors.getOrDefault(name, null);
        if (compressor == null)
            logger.warning(String.format("No such compressor: '%s'", name));
        return compressor;
    }

    public static void registerPresets() {

        // TODO: These should be read from config files!
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_BEST", "1. BasisU Best", "BasisU");
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 0);
            preset.setOptionInt("rdo_uastc_dict_size", 4096);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 3.0f);

            // System.out.printf("PRESET:\n%s\n", preset.toString());
            presets.put(preset.getName(), preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_HIGH", "2. BasisU High", "BasisU");
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 1);  // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h
            preset.setOptionInt("rdo_uastc_dict_size", 8192);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 1.0f);

            // System.out.printf("PRESET:\n%s\n", preset.toString());
            presets.put(preset.getName(), preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_NORMAL", "3. BasisU Normal", "BasisU");
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 2); // cPackUASTCLevelDefault = 2 from basisu_uastc_enc.h

            // System.out.printf("PRESET:\n%s\n", preset.toString());
            presets.put(preset.getName(), preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_FAST", "4. BasisU Fast", "BasisU");
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 1); // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h

            // System.out.printf("PRESET:\n%s\n", preset.toString());
            presets.put(preset.getName(), preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("DEFAULT", "Default (no compression)", "Default");
            // System.out.printf("PRESET:\n%s\n", preset);
            presets.put(preset.getName(), preset);
        }
    }

    public static TextureCompressorPreset getPreset(String name) {
        TextureCompressorPreset preset = presets.getOrDefault(name, null);
        if (preset == null)
            logger.warning(String.format("No such preset: '%s'", name));
        return preset;
    }

}
