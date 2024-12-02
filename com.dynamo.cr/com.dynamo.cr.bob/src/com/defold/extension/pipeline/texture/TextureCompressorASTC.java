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
public class TextureCompressorASTC implements ITextureCompressor {

    public TextureCompressorASTC() {
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_DEFAULT", "ASTC Default", "ASTC");

            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return "ASTC";
    }

    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input)
    {
        System.out.printf(String.format("Compressing using compressor '%s' and preset '%s'\n", getName(), preset.getName()));

        Texc.ASTCEncodeSettings settings = new Texc.ASTCEncodeSettings();
        settings.path = params.getPath();
        settings.width = params.getWidth();
        settings.height = params.getHeight();

        //settings.pixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormat());
        //settings.outPixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormatOut());
        //settings.colorSpace = Texc.ColorSpace.fromValue(params.getColorSpace());

        //settings.numThreads = 4;
        //settings.debug = false;
        settings.data = input;

        /*
        settings.rdo_uastc = preset.getOptionInt("rdo_uastc") != 0;
        settings.pack_uastc_flags = preset.getOptionInt("pack_uastc_flags");
        settings.rdo_uastc_dict_size = preset.getOptionInt("rdo_uastc_dict_size");
        settings.rdo_uastc_quality_scalar = preset.getOptionFloat("rdo_uastc_quality_scalar");
        */

        return TexcLibraryJni.ASTCEncode(settings);
    }
}


