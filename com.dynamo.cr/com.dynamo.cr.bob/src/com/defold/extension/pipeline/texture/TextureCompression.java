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


import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.TextureGenerator;

import java.util.ArrayList;
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

        // Debug
        // logger.info(String.format("Registered texture compressor: '%s'", compressor.getName()));
    }

    public static ITextureCompressor getCompressor(String name) {
        ITextureCompressor compressor = compressors.getOrDefault(name, null);
        if (compressor == null) {
            // There should always be a default compressor availale.
            if (name.equals(TextureCompressorUncompressed.TextureCompressorName)) {
                compressor = new TextureCompressorUncompressed();
                registerCompressor(compressor);
            } else {
                logger.warning(String.format("No such compressor: '%s'", name));
            }
        }
        return compressor;
    }

    // Called from the editor to show selectable compressors
    public static String[] getInstalledCompressorNames() {
        return compressors.keySet().toArray(new String[0]);
    }

    // Called from the editor to show selectable presets for a given compressor
    public static String[] getPresetNamesForCompressor(String compressor) {
        ArrayList<String> compressorPresets = new ArrayList<>();
        for (TextureCompressorPreset preset : presets.values()) {
            if (preset.getCompressorName().equals(compressor)) {
                compressorPresets.add(preset.getName());
            }
        }
        return compressorPresets.toArray(new String[0]);
    }

    public static void registerPreset(TextureCompressorPreset preset) {
        presets.put(preset.getName(), preset);
    }

    public static TextureCompressorPreset getPreset(String name) {
        TextureCompressorPreset preset = presets.getOrDefault(name, null);
        if (preset == null)
            logger.warning(String.format("No such preset: '%s'", name));
        return preset;
    }

}
