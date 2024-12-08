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

import com.dynamo.bob.pipeline.Texc;
import com.dynamo.bob.pipeline.TexcLibraryJni;

/**
 * Implementation of our base texture compressor, using ASTC encoding
 */
public class TextureCompressorASTC implements ITextureCompressor {

    public TextureCompressorASTC() {

        // Note: Quality levels are taken from astcenc.h
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_FAST", "ASTC Fast (Quality=10.0)", "ASTC");
            preset.setOptionFloat("astc_quality", 10);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_NORMAL", "ASTC Normal (Quality=60.0)", "ASTC");
            preset.setOptionFloat("astc_quality", 60);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_HIGH", "ASTC High (Quality=98.0)", "ASTC");
            preset.setOptionFloat("astc_quality", 98);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_BEST", "ASTC Best (Quality=100.0)", "ASTC");
            preset.setOptionFloat("astc_quality", 100);
            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return "ASTC";
    }

    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input)
    {
        // Debug
        // System.out.printf(String.format("Compressing using compressor '%s' and preset '%s'\n", getName(), preset.getName()));

        Texc.ASTCEncodeSettings settings = new Texc.ASTCEncodeSettings();
        settings.path = params.getPath();
        settings.width = params.getWidth();
        settings.height = params.getHeight();
        settings.pixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormat());
        settings.colorSpace = Texc.ColorSpace.fromValue(params.getColorSpace());
        settings.numThreads = 4;
        settings.data = input;

        // ASTC specifics
        settings.qualityLevel = preset.getOptionFloat("astc_quality");

        return TexcLibraryJni.ASTCEncode(settings);
    }
}


