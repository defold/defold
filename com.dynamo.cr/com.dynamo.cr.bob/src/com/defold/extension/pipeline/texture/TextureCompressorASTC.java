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
import com.dynamo.graphics.proto.Graphics.TextureImage;

import java.util.HashMap;

/**
 * Implementation of our base texture compressor, using ASTC encoding
 */
public class TextureCompressorASTC implements ITextureCompressor {

    public static String TextureCompressorName = "ASTC";

    public TextureCompressorASTC() {

        // Note: Quality levels are taken from astcenc.h
        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_FAST", "ASTC Fast (Quality=10.0)", TextureCompressorName);
            preset.setOptionFloat("astc_quality", 10);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_NORMAL", "ASTC Normal (Quality=60.0)", TextureCompressorName);
            preset.setOptionFloat("astc_quality", 60);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_HIGH", "ASTC High (Quality=98.0)", TextureCompressorName);
            preset.setOptionFloat("astc_quality", 98);
            TextureCompression.registerPreset(preset);
        }

        {
            TextureCompressorPreset preset = new TextureCompressorPreset("ASTC_BEST", "ASTC Best (Quality=100.0)", TextureCompressorName);
            preset.setOptionFloat("astc_quality", 100);
            TextureCompression.registerPreset(preset);
        }
    }

    public String getName() {
        return TextureCompressorName;
    }

    private static final HashMap<TextureImage.TextureFormat, int[]> pixelFormatToBlockSize = new HashMap<>();

    static {
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_4x4, new int[] {4,4});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5x4, new int[] {5,4});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5x5, new int[] {5,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6x5, new int[] {6,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6x6, new int[] {6,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8x5, new int[] {8,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8x6, new int[] {8,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8x8, new int[] {8,8});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10x5, new int[] {10,5});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10x6, new int[] {10,6});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10x8, new int[] {10,8});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10x10, new int[] {10,10});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12x10, new int[] {12,10});
        pixelFormatToBlockSize.put(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12x12, new int[] {12,12});
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
        settings.numThreads = 4;
        settings.data = input;
        settings.outPixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormatOut());

        // ASTC specifics
        settings.qualityLevel = preset.getOptionFloat("astc_quality");

        return TexcLibraryJni.ASTCEncode(settings);
    }
}


