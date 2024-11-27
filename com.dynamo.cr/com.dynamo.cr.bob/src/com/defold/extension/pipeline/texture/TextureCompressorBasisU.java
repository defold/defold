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

/**
 * Implementation of our base texture compressor, using BasisU
 */
public class TextureCompressorBasisU implements ITextureCompressor {

    public String getName() {
        return new String("BasisU");
    }

    public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input)
    {
        System.out.printf(String.format("Compressing using compressor '%s' and preset '%s'\n", getName(), preset.getName()));

        Texc.BasisUEncodeSettings settings = new Texc.BasisUEncodeSettings();
        // settings.path = params.getPath();
        // settings.width = params.getWidth();
        // settings.height = params.getHeight();

        // settings.pixelFormat = params.getWidth();
        // settings.outPixelFormat = outPixelFormat; ?
        // settings.colorSpace = colorSpace;

        // settings.numThreads = maxThreads;
        // settings.debug = false;

        // // CL_BEST
        // settings.rdo_uastc = true;
        // settings.pack_uastc_flags = 0;
        // settings.rdo_uastc_dict_size = 4096;
        // settings.rdo_uastc_quality_scalar = 3.0f;

        // DebugPrintObject(settings, 1);

        // settings.data = uncompressed;

        // timeStart = System.currentTimeMillis();

        // encoded = TexcLibraryJni.BasisUEncode(settings);
        // if (encoded == null || encoded.length == 0) {
        // throw new Exception("Could not encode");
        // }

        return null;
    }
}


