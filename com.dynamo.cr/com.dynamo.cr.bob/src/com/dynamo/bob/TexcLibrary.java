// Copyright 2020-2022 The Defold Foundation
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

package com.dynamo.bob;

import java.io.File;
import java.nio.Buffer;

import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class TexcLibrary {

    static final String LIBRARY_NAME = "texc_shared";

    static {
        try {
            // Extract and append Bob bundled texc_shared path.
            Platform platform = Platform.getJavaPlatform();
            File lib = new File(Bob.getLib(platform, LIBRARY_NAME));

            String libPath = lib.getParent();
            Bob.addToPaths(libPath);
            Native.register(LIBRARY_NAME);
        } catch (Exception e) {
            System.out.println("FATAL: " + e.getMessage());
        }
    }

    public interface PixelFormat {
        public static int L8                = 0;
        public static int R8G8B8            = 1;
        public static int R8G8B8A8          = 2;
        public static int A8B8G8R8          = 3;
        public static int RGB_PVRTC_2BPPV1  = 4;
        public static int RGB_PVRTC_4BPPV1  = 5;
        public static int RGBA_PVRTC_2BPPV1 = 6;
        public static int RGBA_PVRTC_4BPPV1 = 7;
        public static int RGB_ETC1          = 8;
        public static int R5G6B5            = 9;
        public static int R4G4B4A4          = 10;
        public static int L8A8              = 11;

        public static int RGBA_ETC2         = 12;
        public static int RGBA_ASTC_4x4     = 13;

        public static int RGB_BC1           = 14;
        public static int RGBA_BC3          = 15;
        public static int R_BC4             = 16;
        public static int RG_BC5            = 17;
        public static int RGBA_BC7          = 18;
    }

    public interface ColorSpace {
        public static int LRGB = 0;
        public static int SRGB = 1;
    }

    public interface CompressionLevel {
        public static int CL_FAST    = 0;
        public static int CL_NORMAL  = 1;
        public static int CL_HIGH    = 2;
        public static int CL_BEST    = 3;
    }

    public interface CompressionType {
        public static int CT_DEFAULT    = 0;
        public static int CT_BASIS_UASTC= 3;
        public static int CT_BASIS_ETC1S= 4;
    }

    public enum FlipAxis {
        FLIP_AXIS_X(0),
        FLIP_AXIS_Y(1),
        FLIP_AXIS_Z(2);

        private final int value;
        FlipAxis(int value) { this.value = value; }
        public int getValue() { return this.value; }
    }

    public interface DitherType {
        public static int DT_NONE = 0;
        public static int DT_DEFAULT = 1;
    }

    // Name is used when debugging (usually the path to the resource)
    public static native Pointer TEXC_Create(String name, int width, int height, int pixelFormat, int colorSpace, int compressionType, Buffer data);
    public static native void TEXC_Destroy(Pointer texture);

    public static native int TEXC_GetDataSizeCompressed(Pointer texture, int minMap);
    public static native int TEXC_GetDataSizeUncompressed(Pointer texture, int minMap);
    public static native int TEXC_GetTotalDataSize(Pointer texture);
    public static native int TEXC_GetData(Pointer texture, Buffer outData, int maxOutDataSize);
    public static native int TEXC_GetCompressionFlags(Pointer texture);

    public static native boolean TEXC_Resize(Pointer texture, int width, int height);
    public static native boolean TEXC_PreMultiplyAlpha(Pointer texture);
    public static native boolean TEXC_GenMipMaps(Pointer texture);
    public static native boolean TEXC_Flip(Pointer texture, int flipAxis);
    public static native boolean TEXC_Encode(Pointer texture, int pixelFormat, int colorSpace, int compressionLevel, int compressionType, boolean mipmaps, int num_threads);

    // For font glyphs
    public static native Pointer TEXC_CompressBuffer(Buffer data, int datasize);
    public static native int TEXC_GetTotalBufferDataSize(Pointer buffer);
    public static native int TEXC_GetBufferData(Pointer buffer, Buffer outData, int maxOutDataSize);
    public static native void TEXC_DestroyBuffer(Pointer buffer);

}
