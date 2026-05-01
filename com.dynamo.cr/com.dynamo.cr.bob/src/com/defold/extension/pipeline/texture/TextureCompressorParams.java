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

/**
 * NOTE: Public API for ITextureCompressor
 */
public class TextureCompressorParams {
    private String      path;
    private int         mipMapLevel;
    private int         width;
    private int         height;
    private int         depth;          // 1 for regular texture
    private int         numChannels;
    private int         pixelFormat;    // Corresponds to Texc.PixelFormat enum
    private int         pixelFormatOut; // Corresponds to Texc.PixelFormat enum
    private int         colorSpace;     // Corresponds to Texc.ColorFormat enum

    public TextureCompressorParams(String path, int mipMapLevel, int width, int height, int depth, int numChannels, int pixelFormat, int pixelFormatOut, int colorSpace) {
        this.path = path;
        this.mipMapLevel = mipMapLevel;
        this.width = width;
        this.height = height;
        this.depth = depth;
        this.numChannels = numChannels;
        this.pixelFormat = pixelFormat;
        this.pixelFormatOut = pixelFormatOut;
        this.colorSpace = colorSpace;
    }

    public String getPath() {
        return path;
    }

    public int getMipMapLevel() {
        return mipMapLevel;
    }

    public int getNumChannels() {
        return numChannels;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public int getDepth() {
        return depth;
    }

    public int getPixelFormat() {return pixelFormat;}

    public int getPixelFormatOut() {return pixelFormatOut;}

    public int getColorSpace() {return colorSpace;}
}
