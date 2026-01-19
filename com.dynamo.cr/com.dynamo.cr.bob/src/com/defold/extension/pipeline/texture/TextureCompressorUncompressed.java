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
import com.dynamo.graphics.proto.Graphics;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;

import java.util.ArrayList;

/**
 * Implementation of our base texture compressor, with NO compression
 */
public class TextureCompressorUncompressed implements ITextureCompressor {
    public static String TextureCompressorName = "Uncompressed";
    public static String TextureCompressorUncompressedPresetName = "UNCOMPRESSED";

    private static final ArrayList<TextureFormat> supportedTextureFormats = new ArrayList<>();
    static {
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_RGB);
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_RGBA);
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_RGB_16BPP);
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP);
        supportedTextureFormats.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA);
    }

    public static String GetMigratedCompressionPreset() {
        return TextureCompressorUncompressedPresetName;
    }

    public TextureCompressorUncompressed() {
        TextureCompression.registerPreset(new TextureCompressorPreset(TextureCompressorUncompressedPresetName, "Uncompressed", TextureCompressorName));
    }

    @Override
    public String getName() {
        return TextureCompressorName;
    }

    @Override
    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input) {

        // Fast-path: native DefaultEncode just memcpy's when output format is RGBA8888.
        // Avoids an extra alloc+copy for large textures.
        if (params.getPixelFormat() == params.getPixelFormatOut() &&
            params.getPixelFormat() == Texc.PixelFormat.PF_R8G8B8A8.getValue()) {
            return input;
        }

        Texc.DefaultEncodeSettings settings = new Texc.DefaultEncodeSettings();
        settings.path = params.getPath();
        settings.width = params.getWidth();
        settings.height = params.getHeight();

        settings.pixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormat());
        settings.outPixelFormat = Texc.PixelFormat.fromValue(params.getPixelFormatOut());
        settings.colorSpace = Texc.ColorSpace.fromValue(params.getColorSpace());

        settings.numThreads = 4; // maxThreads;
        settings.debug = false;
        settings.data = input;

        return TexcLibraryJni.DefaultEncode(settings);
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
        // Presets are not used for the default compressor
        return true;
    }
}
