package com.dynamo.bob;

import java.io.File;
import java.nio.Buffer;

import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class TexcLibrary {
    static {
        String libDir = Bob.getTexcLibDir();
        String prop = "jna.library.path";
        System.setProperty(prop, System.getProperty(prop) + File.pathSeparator + libDir);
        Native.register("texc_shared");
    }

    public interface PixelFormat {
        public static int L8 = 0;
        public static int R8G8B8 = 1;
        public static int R8G8B8A8 = 2;
    }

    public interface ColorSpace {
        public static int LRGB = 0;
        public static int SRGB = 1;
    }

    public static native Pointer TEXC_Create(int width, int height, int pixelFormat, int colorSpace, Buffer data);
    public static native void TEXC_Destroy(Pointer texture);

    public static native int TEXC_GetData(Pointer texture, Buffer outData, int maxOutDataSize);

    public static native boolean TEXC_Resize(Pointer texture, int width, int height);
    public static native boolean TEXC_PreMultiplyAlpha(Pointer texture);
    public static native boolean TEXC_GenMipMaps(Pointer texture);
    public static native boolean TEXC_Transcode(Pointer texture, int pixelFormat, int colorSpace);
}
