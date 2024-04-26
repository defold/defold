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

package com.dynamo.bob;

import java.io.IOException;
import java.io.File;
import java.io.ByteArrayOutputStream;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.awt.image.DataBufferByte;

import com.sun.jna.Native;
import com.sun.jna.Pointer;
import java.lang.reflect.Method;

import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;

public class TexcLibrary {

    static final String LIBRARY_NAME = "texc_shared";

    static {
        Class<?> clsbob = null;

        try {
            ClassLoader clsloader = ClassLoader.getSystemClassLoader();
            clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
        } catch (Exception e) {
            System.out.printf("Didn't find Bob class in default system class loader: %s\n", e);
        }

        if (clsbob == null) {
            try {
                // ClassLoader.getSystemClassLoader() doesn't work with junit
                ClassLoader clsloader = TexcLibrary.class.getClassLoader();
                clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
            } catch (Exception e) {
                System.out.printf("Didn't find Bob class in default test class loader: %s\n", e);
            }
        }

        if (clsbob != null)
        {
            try {
                Method getSharedLib = clsbob.getMethod("getSharedLib", String.class);
                Method addToPaths = clsbob.getMethod("addToPaths", String.class);
                File lib = (File)getSharedLib.invoke(null, LIBRARY_NAME);
                String libPath = lib.getParent();
                addToPaths.invoke(null, libPath);
                Native.register(LIBRARY_NAME);
            } catch (Exception e) {
                System.err.printf("Failed to find functions in Bob: %s\n", e);
                System.exit(1);
            }
        }
        else {
            try {
                System.out.printf("Fallback to regular System.register(%s)\n", LIBRARY_NAME);
                Native.register(LIBRARY_NAME);
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                System.exit(1);
            }
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


    private static byte[] toByteArray(BufferedImage bi, String format) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ImageIO.write(bi, format, baos);
        byte[] bytes = baos.toByteArray();
        return bytes;

    }

    private static byte[] imageToBytes(BufferedImage bi) {
        return ((DataBufferByte) bi.getData().getDataBuffer()).getData();
    }

    private static ByteBuffer imageToByteBuffer(BufferedImage bi) {
        return ByteBuffer.wrap(imageToBytes(bi));
    }

    private static int getPixelFormat(BufferedImage bi) {
        switch(bi.getType())
        {
            case BufferedImage.TYPE_INT_RGB:        return PixelFormat.R8G8B8;
            case BufferedImage.TYPE_INT_ARGB:       return PixelFormat.A8B8G8R8;
            case BufferedImage.TYPE_USHORT_565_RGB: return PixelFormat.R5G6B5;
            case BufferedImage.TYPE_BYTE_GRAY:      return PixelFormat.L8;
        }
        return PixelFormat.R8G8B8;
    }

    // Used for testing the importer. Usage:
    //   ./src/com/dynamo/bob/pipeline/test_model_importer.sh <model path>
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");
        // TODO: Add stand alone capabilities for faster testing of apis etc

        if (args.length < 1) {
            System.out.printf("No image input specified!\n");
            return;
        }

        File file = new File(args[0]);
        BufferedImage image = ImageIO.read(file);
        ByteBuffer buffer = imageToByteBuffer(image);
        int pixelFormat = getPixelFormat(image);
        int colorSpace = ColorSpace.SRGB;//getColorSpace(image);
        int compressionType = CompressionType.CT_BASIS_UASTC;

        Pointer pointer = TEXC_Create(file.getName(), image.getWidth(), image.getHeight(), pixelFormat, colorSpace, compressionType, buffer);
        System.out.printf("Created texture '%s'\n", file.getName());
        TEXC_Destroy(pointer);
    }
}
