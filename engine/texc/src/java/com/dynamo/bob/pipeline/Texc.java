// generated, do not edit

package com.dynamo.bob.pipeline;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class Texc {
    public enum PixelFormat {
        PF_L8(0),
        PF_R8G8B8(1),
        PF_R8G8B8A8(2),
        PF_A8B8G8R8(3),
        PF_RGB_PVRTC_2BPPV1(4),
        PF_RGB_PVRTC_4BPPV1(5),
        PF_RGBA_PVRTC_2BPPV1(6),
        PF_RGBA_PVRTC_4BPPV1(7),
        PF_RGB_ETC1(8),
        PF_R5G6B5(9),
        PF_R4G4B4A4(10),
        PF_L8A8(11),
        PF_RGBA_ETC2(12),
        PF_RGBA_ASTC_4x4(13),
        PF_RGB_BC1(14),
        PF_RGBA_BC3(15),
        PF_R_BC4(16),
        PF_RG_BC5(17),
        PF_RGBA_BC7(18);
        private final int value;
        private PixelFormat(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public PixelFormat fromValue(int value) throws IllegalArgumentException {
            for (PixelFormat e : PixelFormat.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to PixelFormat: %d", value) );
        }
    };

    public enum ColorSpace {
        CS_LRGB(0),
        CS_SRGB(1);
        private final int value;
        private ColorSpace(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public ColorSpace fromValue(int value) throws IllegalArgumentException {
            for (ColorSpace e : ColorSpace.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to ColorSpace: %d", value) );
        }
    };

    public enum CompressionLevel {
        CL_FAST(0),
        CL_NORMAL(1),
        CL_HIGH(2),
        CL_BEST(3),
        CL_ENUM(4);
        private final int value;
        private CompressionLevel(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public CompressionLevel fromValue(int value) throws IllegalArgumentException {
            for (CompressionLevel e : CompressionLevel.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to CompressionLevel: %d", value) );
        }
    };

    public enum CompressionType {
        CT_DEFAULT(0),
        CT_WEBP(1),
        CT_WEBP_LOSSY(2),
        CT_BASIS_UASTC(3),
        CT_BASIS_ETC1S(4);
        private final int value;
        private CompressionType(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public CompressionType fromValue(int value) throws IllegalArgumentException {
            for (CompressionType e : CompressionType.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to CompressionType: %d", value) );
        }
    };

    public enum CompressionFlags {
        CF_ALPHA_CLEAN(1);
        private final int value;
        private CompressionFlags(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public CompressionFlags fromValue(int value) throws IllegalArgumentException {
            for (CompressionFlags e : CompressionFlags.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to CompressionFlags: %d", value) );
        }
    };

    public enum FlipAxis {
        FLIP_AXIS_X(0),
        FLIP_AXIS_Y(1),
        FLIP_AXIS_Z(2);
        private final int value;
        private FlipAxis(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public FlipAxis fromValue(int value) throws IllegalArgumentException {
            for (FlipAxis e : FlipAxis.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to FlipAxis: %d", value) );
        }
    };

    public static class Image {
        public String path;
        public byte[] data;
        public int width = 0;
        public int height = 0;
        public PixelFormat pixelFormat = PixelFormat.PF_L8;
        public ColorSpace colorSpace = ColorSpace.CS_LRGB;
    };
    public static class Buffer {
        public byte[] data;
        public int width = 0;
        public int height = 0;
        public boolean isCompressed = false;
    };
    public static class BasisUEncodeSettings {
        public String path;
        public int width = 0;
        public int height = 0;
        public PixelFormat pixelFormat = PixelFormat.PF_L8;
        public ColorSpace colorSpace = ColorSpace.CS_LRGB;
        public byte[] data;
        public int numThreads = 0;
        public boolean debug = false;
        public PixelFormat outPixelFormat = PixelFormat.PF_L8;
        public boolean rdo_uastc = false;
        public int pack_uastc_flags = 0;
        public int rdo_uastc_dict_size = 0;
        public float rdo_uastc_quality_scalar = 0.0f;
    };
    public static class DefaultEncodeSettings {
        public String path;
        public int width = 0;
        public int height = 0;
        public PixelFormat pixelFormat = PixelFormat.PF_L8;
        public ColorSpace colorSpace = ColorSpace.CS_LRGB;
        public byte[] data;
        public int numThreads = 0;
        public boolean debug = false;
        public PixelFormat outPixelFormat = PixelFormat.PF_L8;
    };
}

