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
import com.dynamo.graphics.proto.Graphics.TextureImage;

import java.util.HashMap;

/**
 * Implementation of our base texture compressor, using ASTC encoding
 */
public class TextureCompressorASTC implements ITextureCompressor {

    public static String TextureCompressorName = "ASTC";
    private static final String TexturePresetQualityKey = "astc_quality";

    public static String GetMigratedCompressionPreset(Graphics.TextureFormatAlternative.CompressionLevel level) {
        if (level == null)
            level = Graphics.TextureFormatAlternative.CompressionLevel.FAST;
        return switch (level) {
            case FAST -> "ASTC_QUALITY_10";
            case NORMAL -> "ASTC_QUALITY_60";
            case HIGH -> "ASTC_QUALITY_98";
            case BEST -> "ASTC_QUALITY_100";
        };
    }

    public TextureCompressorASTC() {

        // Note: Quality levels are taken from astcenc.h
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_QUALITY_10", "Low (Quality=10.0)", TextureCompressorName);
            preset.setOptionFloat(TexturePresetQualityKey, 10);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_QUALITY_60", "Medium (Quality=60.0)", TextureCompressorName);
            preset.setOptionFloat(TexturePresetQualityKey, 60);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_QUALITY_98", "High (Quality=98.0)", TextureCompressorName);
            preset.setOptionFloat(TexturePresetQualityKey, 98);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_QUALITY_100", "Highest (Quality=100.0)", TextureCompressorName);
            preset.setOptionFloat(TexturePresetQualityKey, 100);
            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return TextureCompressorName;
    }

    private static final HashMap<TextureImage.TextureFormat, int[]> pixelFormatToBlockSize = new HashMap<>();

    static {
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_4X4, new int[] {4,4});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5X4, new int[] {5,4});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5X5, new int[] {5,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6X5, new int[] {6,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6X6, new int[] {6,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X5, new int[] {8,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X6, new int[] {8,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X8, new int[] {8,8});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X5, new int[] {10,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X6, new int[] {10,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X8, new int[] {10,8});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X10, new int[] {10,10});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12X10, new int[] {12,10});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12X12, new int[] {12,12});
    }

    public boolean supportsTextureFormat(TextureImage.TextureFormat format) {
        if (format == null) {
            return false;
        }
        return pixelFormatToBlockSize.containsKey(format);
    }

    public boolean supportsTextureCompressorPreset(TextureCompressorPreset preset) {
        Float f = preset.getOptionFloat(TexturePresetQualityKey);
        return f != null;
    }

    public int getAlignedWidth(TextureImage.TextureFormat format, int width) {
        int[] blockSizes = pixelFormatToBlockSize.get(format);

        if (blockSizes == null) {
            System.err.println("Format " + format + " is not an ASTC format.");
            return 0;
        }

        int blockSizeX = blockSizes[0];
        return ((width + blockSizeX - 1) / blockSizeX) * blockSizeX;
    }

    public int getAlignedHeight(TextureImage.TextureFormat format, int height) {
        int[] blockSizes = pixelFormatToBlockSize.get(format);

        if (blockSizes == null) {
            System.err.println("Format " + format + " is not an ASTC format.");
            return 0;
        }

        int blockSizeY = blockSizes[1];
        return ((height + blockSizeY - 1) / blockSizeY) * blockSizeY;
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
        settings.numThreads = TextureGenerator.maxThreads;
        settings.data = input;
        settings.outPixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormatOut());

        // ASTC specifics
        settings.qualityLevel = preset.getOptionFloat(TexturePresetQualityKey);

        return TexcLibraryJni.ASTCEncode(settings);
    }
}


