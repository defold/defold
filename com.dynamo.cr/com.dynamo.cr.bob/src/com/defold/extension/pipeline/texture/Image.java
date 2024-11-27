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

import com.dynamo.bob.pipeline.Texc.ColorSpace;
import com.dynamo.bob.pipeline.Texc.PixelFormat;

/**
 * Interface for compressing a texture using settings
 * NOTE: Public API for ITextureCompressor
 * We will scan for public non-abstract classes that implement this interface inside the com.defold.extension.pipeline
 * package and instantiate them to transform Lua source code. Implementors must provide a no-argument constructor.
 */
public class Image {
    private String      path;       // For easier debugging
    private int         mipLevel;
    private int         width;
    private int         height;
    private int         depth;
    private PixelFormat pixelFormat;
    private ColorSpace  colorSpace;
    private byte[]      data;

    public Image(String path, int mipLevel, int width, int height, int depth, PixelFormat pixelFormat, ColorSpace colorSpace, byte[] data) {
        this.path       = path;
        this.mipLevel   = mipLevel;
        this.width      = width;
        this.height     = height;
        this.depth      = depth;
        this.pixelFormat= pixelFormat;
        this.colorSpace = colorSpace;
        this.data       = data;
    }

    public String getPath() {
        return path;
    }
    public int getMipLevel() {
        return mipLevel;
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
    public PixelFormat getPixelFormat() {
        return pixelFormat;
    }
    public ColorSpace getColorSpace() {
        return colorSpace;
    }
    public byte[] getData() {
        return data;
    }

    public String toString() {
        return String.format("Image:\n  path: '%s'\n  mip: %d\n  dims: %d x %d x %d,\n  pf: %s,\n  cs: %s\n  size: %d\n",
                                        path, mipLevel, width, height, depth, pixelFormat.toString(), colorSpace.toString(), data.length);
    }
}
