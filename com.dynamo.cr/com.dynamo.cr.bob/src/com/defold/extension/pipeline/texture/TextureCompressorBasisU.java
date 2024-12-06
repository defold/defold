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
 * Implementation of our base texture compressor, using BasisU
 */
public class TextureCompressorBasisU implements ITextureCompressor {

    public TextureCompressorBasisU() {
        // TODO: These should be read from config files!
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_BEST", "1. BasisU Best", "BasisU");
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 0);
            preset.setOptionInt("rdo_uastc_dict_size", 4096);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 3.0f);

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_HIGH", "2. BasisU High", "BasisU");
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 1);  // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h
            preset.setOptionInt("rdo_uastc_dict_size", 8192);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 1.0f);

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_NORMAL", "3. BasisU Normal", "BasisU");
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 2); // cPackUASTCLevelDefault = 2 from basisu_uastc_enc.h

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_FAST", "4. BasisU Fast", "BasisU");
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 1); // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h

            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return "BasisU";
    }

    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input)
    {
        // Debug:
        // System.out.printf(String.format("Compressing using compressor '%s' and preset '%s'\n", getName(), preset.getName()));

        Texc.BasisUEncodeSettings settings = new Texc.BasisUEncodeSettings();
        settings.path = params.getPath();
        settings.width = params.getWidth();
        settings.height = params.getHeight();

        settings.pixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormat());
        settings.outPixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormatOut());
        settings.colorSpace = Texc.ColorSpace.fromValue(params.getColorSpace());

        settings.numThreads = 4;
        settings.debug = false;
        settings.data = input;

        settings.rdo_uastc = preset.getOptionInt("rdo_uastc") != 0;
        settings.pack_uastc_flags = preset.getOptionInt("pack_uastc_flags");

        Integer dictSize = preset.getOptionInt("rdo_uastc_dict_size");
        if (dictSize != null) {
            settings.rdo_uastc_dict_size = dictSize;
        }

        Float qualityScalar = preset.getOptionFloat("rdo_uastc_quality_scalar");
        if (qualityScalar != null) {
            settings.rdo_uastc_quality_scalar = qualityScalar;
        }

        return TexcLibraryJni.BasisUEncode(settings);
    }
}


