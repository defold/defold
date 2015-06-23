package com.defold.libs;

import java.nio.Buffer;

import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class TexcLibrary {
    static {
        try {
            String libDir = Libs.getTexcLibDir();
            System.setProperty("jna.library.path", libDir);
            Native.register("texc_shared");
        } catch (Exception e) {
            System.out.println("FATAL: " + e.getMessage());
        }
    }

    public interface PixelFormat {
        public static int L8                = 0;
        public static int R8G8B8            = 1;
        public static int R8G8B8A8          = 2;
        public static int RGB_PVRTC_2BPPV1  = 3;
        public static int RGB_PVRTC_4BPPV1  = 4;
        public static int RGBA_PVRTC_2BPPV1 = 5;
        public static int RGBA_PVRTC_4BPPV1 = 6;
        public static int RGB_ETC1          = 7;
        /*
        JIRA issue: DEF-994
        public static int RGB_DXT1          = 8;
        public static int RGBA_DXT1         = 9;
        public static int RGBA_DXT3         = 10;
        public static int RGBA_DXT5         = 11;
        */
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

    public static native Pointer TEXC_Create(int width, int height, int pixelFormat, int colorSpace, Buffer data);
    public static native void TEXC_Destroy(Pointer texture);

    public static native int TEXC_GetDataSize(Pointer texture, int minMap);
    public static native int TEXC_GetData(Pointer texture, Buffer outData, int maxOutDataSize);

    public static native boolean TEXC_Resize(Pointer texture, int width, int height);
    public static native boolean TEXC_PreMultiplyAlpha(Pointer texture);
    public static native boolean TEXC_GenMipMaps(Pointer texture);
    public static native boolean TEXC_Transcode(Pointer texture, int pixelFormat, int colorSpace, int compressionLevel);

}
