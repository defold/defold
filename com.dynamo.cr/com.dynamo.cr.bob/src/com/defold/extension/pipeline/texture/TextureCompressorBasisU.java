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

import com.dynamo.bob.pipeline.Texc;
import com.dynamo.bob.pipeline.TexcLibraryJni;
import com.dynamo.bob.pipeline.TextureGenerator;
import com.dynamo.graphics.proto.Graphics;

import java.util.ArrayList;

/**
 * Implementation of our base texture compressor, using BasisU
 */
public class TextureCompressorBasisU implements ITextureCompressor {

    public static String TextureCompressorName = "BasisU";

    private static final ArrayList<Graphics.TextureImage.TextureFormat> supportedTextureFormats = new ArrayList<>();
    static {
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_RGB);
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA);
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_RGB_16BPP);
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_16BPP);
        supportedTextureFormats.add(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA);
    }

    public static String GetMigratedCompressionPreset(Graphics.TextureFormatAlternative.CompressionLevel level) {
        if (level == null)
            level = Graphics.TextureFormatAlternative.CompressionLevel.FAST;
        return switch (level) {
            case FAST -> "BASISU_LOW";
            case NORMAL -> "BASISU_MEDIUM";
            case HIGH -> "BASISU_HIGH";
            case BEST -> "BASISU_HIGHEST";
        };
    }

    public TextureCompressorBasisU() {
        // TODO: These should be read from config files!
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_HIGHEST", "BasisU Highest", TextureCompressorName);
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 0);
            preset.setOptionInt("rdo_uastc_dict_size", 4096);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 3.0f);

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_HIGH", "BasisU High", TextureCompressorName);
            preset.setOptionInt("rdo_uastc", 1);
            preset.setOptionInt("pack_uastc_flags", 1);  // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h
            preset.setOptionInt("rdo_uastc_dict_size", 8192);
            preset.setOptionFloat("rdo_uastc_quality_scalar", 1.0f);

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_MEDIUM", "BasisU Medium", TextureCompressorName);
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 2); // cPackUASTCLevelDefault = 2 from basisu_uastc_enc.h

            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("BASISU_LOW", "BasisU Low", TextureCompressorName);
            preset.setOptionInt("rdo_uastc", 0);
            preset.setOptionInt("pack_uastc_flags", 1); // cPackUASTCLevelFaster = 1 from basisu_uastc_enc.h

            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return TextureCompressorName;
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

        // increasing of threads count for basisu isn't that efficient, so keep it 4 for now
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

    @Override
    public boolean supportsTextureFormat(Graphics.TextureImage.TextureFormat format) {
        if (format == null) {
            return false;
        }
        return supportedTextureFormats.contains(format);
    }

    @Override
    public boolean supportsTextureCompressorPreset(TextureCompressorPreset preset) {
        Integer rdo_uastc = preset.getOptionInt("rdo_uastc");
        Integer pack_uastc_flags = preset.getOptionInt("pack_uastc_flags");
        return rdo_uastc != null && pack_uastc_flags != null;
    }
}


