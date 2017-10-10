package com.dynamo.bob;

import java.io.File;
import java.nio.Buffer;

import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class TexcLibrary {
    static {
        try {
            String jnaLibraryPath = null;

            // Check if jna.library.path is set externally.
            if (System.getProperty("jna.library.path") != null) {
                jnaLibraryPath = System.getProperty("jna.library.path");
            }

            // Extract and append Bob bundled texc_shared path.
            Platform platform = Platform.getJavaPlatform();
            File lib = new File(Bob.getLib(platform, "texc_shared"));

            // TODO: sad with a platform specific hack and placing dependency knowledge here but...
            if (platform == Platform.X86Darwin || platform == Platform.X86_64Darwin || platform == Platform.X86Linux || platform == Platform.X86_64Linux) {
                Bob.getLib(platform, "PVRTexLib");
            }
            else if (platform == Platform.X86_64Win32 || platform == Platform.X86Win32) {
                Bob.getLib(platform, "PVRTexLib");
                Bob.getLib(platform, "msvcr120"); // dependency of PVRTexLib
            }

            if (jnaLibraryPath == null) {
                // Set path where texc_shared library is found.
                jnaLibraryPath = lib.getParent();
            } else {
                // Append path where texc_shared library is found.
                jnaLibraryPath += File.pathSeparator + lib.getParent();
            }

            // Set the concatenated jna.library path
            System.setProperty("jna.library.path", jnaLibraryPath);
            Bob.verbose("Set jna.library.path to '%s'", jnaLibraryPath);

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
        public static int R5G6B5            = 8;
        public static int R4G4B4A4          = 9;
        public static int L8A8              = 10;

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

    public interface CompressionType {
        public static int CT_DEFAULT    = 0;
        public static int CT_WEBP       = 1;
        public static int CT_WEBP_LOSSY = 2;
    }

    public interface FlipAxis {
        public static int FLIP_AXIS_X = 0;
        public static int FLIP_AXIS_Y = 1;
        public static int FLIP_AXIS_Z = 2;
    }

    public interface DitherType {
        public static int DT_NONE = 0;
        public static int DT_DEFAULT = 1;
    }

    public static native Pointer TEXC_Create(int width, int height, int pixelFormat, int colorSpace, Buffer data);
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
    public static native boolean TEXC_Transcode(Pointer texture, int pixelFormat, int colorSpace, int compressionLevel, int compressionType, int dither);

}
