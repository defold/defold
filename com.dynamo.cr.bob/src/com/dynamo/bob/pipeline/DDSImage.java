package com.dynamo.bob.pipeline;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.bob.pipeline.DDS.DDSURFACEDESC2;

public class DDSImage {

    static class CompressionFormatKey {
        private String fourCC;
        private boolean hasAlpha;

        public CompressionFormatKey(String fourCC, boolean hasAlpha) {
            this.fourCC = fourCC;
            this.hasAlpha = hasAlpha;
        }

        @Override
        public int hashCode() {
            return fourCC.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof CompressionFormatKey) {
                CompressionFormatKey other = (CompressionFormatKey) obj;
                return fourCC.equals(other.fourCC) && hasAlpha == other.hasAlpha;
            }
            return super.equals(obj);
        }
    }

    private final static long GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 33776;
    private final static long GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 33777;
    private final static long GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 33778;
    private final static long GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 33779;

    private static Map<CompressionFormatKey, Long> compressionFormatMap = new HashMap<DDSImage.CompressionFormatKey, Long>();
    static {
        compressionFormatMap.put(new CompressionFormatKey("DXT1", false), GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        compressionFormatMap.put(new CompressionFormatKey("DXT1", true), GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
        compressionFormatMap.put(new CompressionFormatKey("DXT3", false), GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
        compressionFormatMap.put(new CompressionFormatKey("DXT3", true), GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
        compressionFormatMap.put(new CompressionFormatKey("DXT5", false), GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
        compressionFormatMap.put(new CompressionFormatKey("DXT5", true), GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    }

    private long width;
    private long height;
    private String fourCC;
    private boolean hasAlpha;
    private ArrayList<byte[]> mipMaps;

    public DDSImage(long width, long height, String fourCC, boolean hasAlpha,
            ArrayList<byte[]> mipMaps) {
        this.width = width;
        this.height = height;
        this.fourCC = fourCC;
        this.hasAlpha = hasAlpha;
        this.mipMaps = mipMaps;
    }

    public static DDSImage decode(ByteBuffer buffer) throws DDSException {
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        DDSURFACEDESC2 desc = new DDSURFACEDESC2();
        desc.setByteBuffer(buffer, 0);

        if (!desc.dwMagic.get().equals("DDS ") || desc.dwSize.get() != 124)
            throw new DDSException("Invalid DDS file (incorrect header)");

        long width = desc.dwWidth.get();
        long height = desc.dwHeight.get();
        boolean compressed = false;
        boolean volume = false;
        long mipmaps = 1;

        if ((desc.dwFlags.get() & DDS.DDSD_PITCH) != 0) {
            long pitch = desc.dwPitchOrLinearSize.get();
        }
        else if ((desc.dwFlags.get() & DDS.DDSD_LINEARSIZE) != 0) {
            long image_size = desc.dwPitchOrLinearSize.get();
            compressed = true;
        }

        if ((desc.dwFlags.get() & DDS.DDSD_DEPTH) != 0) {
            throw new DDSException("Volume DDS files unsupported");
            /*volume = true;
            long depth = desc.dwDepth.get();*/
        }

        if ((desc.dwFlags.get() & DDS.DDSD_MIPMAPCOUNT) != 0) {
            mipmaps = desc.dwMipMapCount.get();
        }

        if (desc.ddpfPixelFormat.dwSize.get() != 32) {
            throw new DDSException("Invalid DDS file (incorrect pixel format).");
        }

        if ((desc.dwCaps2.get() & DDS.DDSCAPS2_CUBEMAP) != 0) {
            throw new DDSException("Cubemap DDS files unsupported");
        }

        if (!((desc.ddpfPixelFormat.dwFlags.get() & DDS.DDPF_FOURCC) != 0)) {
            throw new DDSException("Uncompressed DDS textures not supported.");
        }

        boolean has_alpha = desc.ddpfPixelFormat.dwRGBAlphaBitMask.get() != 0;

        Long format = null;
        format = compressionFormatMap.get(new CompressionFormatKey(desc.ddpfPixelFormat.dwFourCC.get(), has_alpha));
        if (format == null) {
            throw new DDSException(String.format("Unsupported texture compression %s", desc.ddpfPixelFormat.dwFourCC.get()));
        }

        long block_size;
        if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
            block_size = 8;
        }
        else {
            block_size = 16;
        }

        ArrayList<byte[]> datas = new ArrayList<byte[]>();
        long w = width;
        long h = height;
        for (int i = 0; i < mipmaps; ++i) {
            if (w == 0 && h == 0) {
                break;
            }
            if (w == 0) {
                w = 1;
            }
            if (h == 0) {
                h = 1;
            }
            long size = ((w + 3) / 4) * ((h + 3) / 4) * block_size;
            byte[] data = new byte[(int) size];
            buffer.get(data);
            datas.add(data);
            w >>= 1;
            h >>= 1;
        }

        return new DDSImage(width, height, desc.ddpfPixelFormat.dwFourCC.get(), has_alpha, datas);
    }
}
