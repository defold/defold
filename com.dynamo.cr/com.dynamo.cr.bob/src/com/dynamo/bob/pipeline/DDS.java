package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;

import javax.imageio.ImageIO;

import javolution.io.Struct;

public class DDS {

    // dwFlags of DDSURFACEDESC2
    public static final long DDSD_CAPS           = 0x00000001L;
    public static final long DDSD_HEIGHT         = 0x00000002L;
    public static final long DDSD_WIDTH          = 0x00000004L;
    public static final long DDSD_PITCH          = 0x00000008L;
    public static final long DDSD_PIXELFORMAT    = 0x00001000L;
    public static final long DDSD_MIPMAPCOUNT    = 0x00020000L;
    public static final long DDSD_LINEARSIZE     = 0x00080000L;
    public static final long DDSD_DEPTH          = 0x00800000L;

    // ddpfPixelFormat of DDSURFACEDESC2
    public static final long DDPF_ALPHAPIXELS   = 0x00000001L;
    public static final long DDPF_FOURCC        = 0x00000004L;
    public static final long DDPF_RGB           = 0x00000040L;

    // dwCaps1 of DDSCAPS2
    public static final long DDSCAPS_COMPLEX     = 0x00000008L;
    public static final long DDSCAPS_TEXTURE     = 0x00001000L;
    public static final long DDSCAPS_MIPMAP      = 0x00400000L;

    // dwCaps2 of DDSCAPS2
    public static final long DDSCAPS2_CUBEMAP           = 0x00000200L;
    public static final long DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400L;
    public static final long DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800L;
    public static final long DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000L;
    public static final long DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000L;
    public static final long DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000L;
    public static final long DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000L;
    public static final long DDSCAPS2_VOLUME            = 0x00200000L;

    public static class DDSURFACEDESC2 extends Struct {
        public final UTF8String dwMagic = new UTF8String(4);
        public final Unsigned32 dwSize = new Unsigned32();
        public final Unsigned32 dwFlags = new Unsigned32();
        public final Unsigned32 dwHeight = new Unsigned32();
        public final Unsigned32 dwWidth = new Unsigned32();
        public final Unsigned32 dwPitchOrLinearSize = new Unsigned32();
        public final Unsigned32 dwDepth = new Unsigned32();
        public final Unsigned32 dwMipMapCount = new Unsigned32();
        public final UTF8String dwReserved1 = new UTF8String(44);
        public final DDPIXELFORMAT ddpfPixelFormat = inner(new DDPIXELFORMAT());
        public final Unsigned32 dwCaps1 = new Unsigned32();
        public final Unsigned32 dwCaps2 = new Unsigned32();
        public final UTF8String dwCapsReserved = new UTF8String(8);
        public final Unsigned32 dwReserved2 = new Unsigned32();

        @Override
        public ByteOrder byteOrder() {
            return ByteOrder.LITTLE_ENDIAN;
        }
    }

    public static class DDPIXELFORMAT extends Struct {
        public final Unsigned32 dwSize = new Unsigned32();
        public final Unsigned32 dwFlags = new Unsigned32();
        public final UTF8String dwFourCC = new UTF8String(4);
        public final Unsigned32 dwRGBBitCount = new Unsigned32();
        public final Unsigned32 dwRBitMask = new Unsigned32();
        public final Unsigned32 dwGBitMask = new Unsigned32();
        public final Unsigned32 dwBBitMask = new Unsigned32();
        public final Unsigned32 dwRGBAlphaBitMask = new Unsigned32();

        @Override
        public ByteOrder byteOrder() {
            return ByteOrder.LITTLE_ENDIAN;
        }
    }

    public static void main(String[] args) throws IOException {
        DDSURFACEDESC2 s = new DDSURFACEDESC2();
        File f = new File("/tmp/e.dds");
        FileInputStream fis = new FileInputStream(f);
        FileChannel c = fis.getChannel();

        //buf = ByteBuffer.allocate(f.length());
        MappedByteBuffer m = c.map(MapMode.READ_ONLY, 0, 49328);
        m.order(ByteOrder.LITTLE_ENDIAN);
        s.setByteBuffer(m, 0);

        System.out.println(s.ddpfPixelFormat.dwSize);
        int[] buf;
        fis = new FileInputStream("/tmp/gray.png");
        BufferedImage image = ImageIO.read(fis);
        System.out.println(image.getColorModel());
        System.out.println(image.getRaster());
        buf = new int[image.getWidth() * image.getHeight()];
        image.getRaster().getPixels(0, 0, image.getWidth(), image.getHeight(), buf);

        fis = new FileInputStream("/tmp/rgb_alpha.png");
        image = ImageIO.read(fis);
        System.out.println(image.getColorModel());
        System.out.println(image.getRaster());
        System.out.println(image.getRaster().getNumBands());
        System.out.println(image.getRaster().getNumDataElements());
        System.out.println(image.getColorModel().getClass());


        buf = new int[image.getWidth() * image.getHeight() * 4];
        image.getRaster().getPixels(0, 0, image.getWidth(), image.getHeight(), buf);

    }
}
