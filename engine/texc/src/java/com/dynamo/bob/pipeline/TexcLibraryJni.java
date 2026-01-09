//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;
import java.nio.IntBuffer;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.ArrayList;
import java.util.List;

import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.DataBufferShort;
import java.awt.image.DataBufferUShort;
import java.awt.image.DataBufferInt;

import javax.imageio.ImageIO;

public class TexcLibraryJni {

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
                ClassLoader clsloader = TexcLibraryJni.class.getClassLoader();
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
                System.load(lib.getAbsolutePath());
            } catch (InvocationTargetException e) {
                Throwable cause = e.getCause();
                System.err.printf("Failed to find functions in Bob: %s\n", cause.getMessage());
                cause.printStackTrace();
                System.exit(1);
            } catch (Exception e) {
                System.err.printf("Failed to find functions in Bob: %s\n", e);
                e.printStackTrace();
                System.exit(1);
            }
        }
        else {
            try {
                System.out.printf("Fallback to regular System.loadLibrary(%s)\n", LIBRARY_NAME);
                System.loadLibrary(LIBRARY_NAME); // Requires the java.library.path to be set
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                e.printStackTrace();
                System.exit(1);
            }
        }
    }

    // Used by the TextureGenerator.java
    public static native long CreateImage(String name, int width, int height, int pixelFormat, int colorSpace, byte[] data);
    public static native int CreatePreviewImage(int width, int height, byte[] inputArray, byte[] outputArray);
    public static native void DestroyImage(long image);
    public static native byte[] GetData(long image);

    public static native int GetWidth(long image);
    public static native int GetHeight(long image);
    public static native long Resize(long image, int width, int height); // Creates a new image. Call DestroyImage
    public static native boolean PreMultiplyAlpha(long image);
    public static native boolean Flip(long image, int flipAxis);
    public static native boolean Dither(long image, int pixelFormat);

    // Part of the basisu compressor api
    public static native byte[] BasisUEncode(Texc.BasisUEncodeSettings input);
    public static native byte[] ASTCEncode(Texc.ASTCEncodeSettings input);
    public static native byte[] DefaultEncode(Texc.DefaultEncodeSettings input);

    // For font glyphs
    public static native Texc.Buffer CompressBuffer(byte[] data);

    // //////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: TexcLibraryJni.class <image_file>\n");
        System.out.printf("\n");
    }

    public static void PrintArray(Object arr, StringBuilder builder)
    {
        builder.append("[");
        int length = Array.getLength(arr);
        for (int i = 0; i < length; i ++) {
            Object arrayElement = Array.get(arr, i);

            builder.append(arrayElement);
            if (i != (length-1))
            {
                builder.append(", ");
            }
        }
        builder.append("]");
    }

    private static boolean IsTexcClass(Class<?> cls)
    {
        return cls.getName().startsWith("com.dynamo.bob.pipeline.Texc");
    }

    public static void PrintFields(Object obj, Field[] fields, boolean skip_tructs, StringBuilder builder, int indent)
    {
        String newLine = System.getProperty("line.separator");
        for ( Field field : fields  )
        {
            Class field_cls = field.getType();

            boolean is_struct = IsTexcClass(field.getType());
            if (field_cls.isEnum())
                is_struct = false;

            if (skip_tructs && is_struct)
                continue;
            else if (!skip_tructs && !is_struct)
                continue;

            Object field_obj = null;
            try {
                //requires access to private field:
                field.setAccessible(true);
                field_obj = field.get(obj);
            } catch ( IllegalAccessException ex ) {
                System.out.println(ex);
                continue;
            }

            for (int i = 0; i < indent; ++i) {
                builder.append("  ");
            }

            if (is_struct)
            {
                builder.append( field.getName() );
                builder.append(": ");
                if (field_obj == null)
                {
                    builder.append( field_obj );
                    builder.append(newLine);
                }
                else
                {
                    builder.append(newLine);

                    ToString(field_obj, builder, indent+1);
                }
                continue;
            }

            builder.append( field.getName() );
            builder.append(": ");

            if (field_obj == null)
            {
                builder.append( field_obj );
            }
            else if (field_cls.isArray())
            {
                PrintArray(field_obj, builder);
            }
            else
            {
                builder.append( field_obj.toString() );
            }

            builder.append(newLine);
        }
    }

    public static StringBuilder ToString(Object obj, StringBuilder builder, int indent)
    {
        if (obj == null)
        {
            builder.append(obj);
            return builder;
        }

        Class cls = obj.getClass();

        //determine fields declared in this class only (no fields of superclass)
        Field[] fields = cls.getDeclaredFields();
        // Print "simple" fields first
        PrintFields(obj, fields, true, builder, indent);
        PrintFields(obj, fields, false, builder, indent);

        return builder;
    }

    public static void DebugPrintObject(Object object, int indent) {
        StringBuilder builder = new StringBuilder();
        builder = ToString(object, builder, indent+1);
        System.out.printf("%s\n", builder.toString());
    }

    // public static byte[] ReadFile(File file) throws IOException
    // {
    //     InputStream inputStream = new FileInputStream(file);
    //     byte[] bytes = new byte[inputStream.available()];
    //     inputStream.read(bytes);
    //     return bytes;
    // }

    // public static byte[] ReadFile(String path) throws IOException
    // {
    //     return ReadFile(new File(path));
    // }

    private static ByteBuffer getByteBuffer(BufferedImage bi)
    {
        ByteBuffer byteBuffer;
        DataBuffer dataBuffer = bi.getRaster().getDataBuffer();

        if (dataBuffer instanceof DataBufferByte) { // This is the usual case, where data is simply wrapped
            byte[] pixelData = ((DataBufferByte) dataBuffer).getData();
            byteBuffer = ByteBuffer.wrap(pixelData);
        }
        else if (dataBuffer instanceof DataBufferUShort) {
            short[] pixelData = ((DataBufferUShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        }
        else if (dataBuffer instanceof DataBufferShort) {
            short[] pixelData = ((DataBufferShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        }
        else if (dataBuffer instanceof DataBufferInt) {
            int[] pixelData = ((DataBufferInt) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 4);
            byteBuffer.asIntBuffer().put(IntBuffer.wrap(pixelData));
        }
        else {
            throw new IllegalArgumentException("Not implemented for data buffer type: " + dataBuffer.getClass());
        }

        return byteBuffer;
    }

    // Used for testing the importer. Usage:
    //    cd engine/texc
    //   ./scripts/test_texture_compiler.sh <image path (e.g. .png)>
    public static void main(String[] args) throws IOException, Exception {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        int maxThreads = 4;
        boolean premulAlpha = true;
        boolean generateMipMaps = true;

        for (int i = 0; i < args.length; ++i)
        {
            // if (args[i].equalsIgnoreCase("--test-exception"))
            // {
            //     TexcLibraryJni.TestException("Testing exception: " + args[i+1]);
            //     return; // exit code 0
            // }
        }

        String path = args[0];
        System.out.printf("Loading '%s'\n", path);

        long timeStart = System.currentTimeMillis();

        BufferedImage srcimage = null;
        try {
            srcimage = ImageIO.read(new FileInputStream(path));
        } catch (Exception e) {
            System.out.printf("Failed to load image '%s': %s\n", path, e);
            return;
        }

        int width = srcimage.getWidth();
        int height = srcimage.getHeight();

        ByteBuffer byteBuffer = getByteBuffer(srcimage);
        byte[] bytes = byteBuffer.array();
        long timeEnd = System.currentTimeMillis();

        System.out.printf("Loaded '%s' in %d ms\n", path, timeEnd - timeStart);

        List<Long> images = new ArrayList<>();
        int w = width;
        int h = height;
        int mipLevel = 0;
        long firstImage = 0;
        long prevImage = 0;

        System.out.printf("-----------------\n");
        System.out.printf("Generate mip levels\n");

        while (w != 0 || h != 0) {
            w = Math.max(w, 1);
            h = Math.max(h, 1);

            long image = 0;
            if (firstImage == 0)
            {
                timeStart = System.currentTimeMillis();

                firstImage = TexcLibraryJni.CreateImage(path, width, height,
                                            Texc.PixelFormat.PF_A8B8G8R8.getValue(),
                                            Texc.ColorSpace.CS_SRGB.getValue(),
                                            bytes);

                timeEnd = System.currentTimeMillis();

                image = firstImage;
            }
            else
            {
                timeStart = System.currentTimeMillis();

                image = TexcLibraryJni.Resize(prevImage, w, h);

                timeEnd = System.currentTimeMillis();
            }

            if (image == 0) {
                System.out.printf("Failed to create texture for '%s'\n", path);
                return;
            }

            images.add(image);
            prevImage = image;

            if (!generateMipMaps)
                break;

            System.out.printf(" mip %2d in %4d ms  (%d x %d)\n", mipLevel, w, h, timeEnd - timeStart);

            mipLevel++;
            w /= 2;
            h /= 2;
        }

        Texc.PixelFormat pixelFormat = Texc.PixelFormat.PF_R8G8B8A8;
        Texc.PixelFormat outPixelFormat = pixelFormat;
        outPixelFormat = Texc.PixelFormat.PF_R4G4B4A4;

        Texc.ColorSpace colorSpace = Texc.ColorSpace.CS_SRGB;
        Texc.CompressionLevel texcCompressionLevel = Texc.CompressionLevel.CL_BEST;
        //Texc.CompressionType texcCompressionType = Texc.CompressionType.CT_DEFAULT;
        //Texc.CompressionType texcCompressionType = Texc.CompressionType.CT_BASIS_UASTC;

        Texc.CompressionType texcCompressionType = Texc.CompressionType.CT_ASTC;

        if (premulAlpha && !ColorModel.getRGBdefault().isAlphaPremultiplied()) {
            System.out.printf("-----------------\n");
            System.out.printf("PreMultiplyAlpha\n");

            mipLevel = 0;
            for (long image : images)
            {
                timeStart = System.currentTimeMillis();
                if (!TexcLibraryJni.PreMultiplyAlpha(image)) {
                    throw new Exception("could not premultiply alpha");
                }
                timeEnd = System.currentTimeMillis();
                System.out.printf("  mip %2d took %3d ms\n", mipLevel, timeEnd - timeStart);

                mipLevel++;
            }
        }

        // Loop over all axis that should be flipped.
        EnumSet<Texc.FlipAxis> flipAxis = EnumSet.of(Texc.FlipAxis.FLIP_AXIS_Y);
        for (Texc.FlipAxis flip : flipAxis) {
            System.out.printf("-----------------\n");
            System.out.printf("Flip %s\n", flip.toString());

            mipLevel = 0;
            for (long image : images)
            {
                timeStart = System.currentTimeMillis();
                if (!TexcLibraryJni.Flip(image, flip.getValue())) {
                    throw new Exception("could not flip on " + flip.toString());
                }
                timeEnd = System.currentTimeMillis();
                System.out.printf("  Flip mip %2d took %3d ms\n", mipLevel, timeEnd - timeStart);
                mipLevel++;
            }
        }

        if (outPixelFormat == Texc.PixelFormat.PF_R4G4B4A4 || outPixelFormat == Texc.PixelFormat.PF_R5G6B5) {
            System.out.printf("-----------------\n");
            System.out.printf("Dither\n");

            mipLevel = 0;
            for (long image : images)
            {
                timeStart = System.currentTimeMillis();
                if (!TexcLibraryJni.Dither(image, outPixelFormat.getValue())) {
                    throw new Exception("could not dither image");
                }
                timeEnd = System.currentTimeMillis();
                System.out.printf("  Dither mip %2d took %d ms\n", mipLevel, timeEnd - timeStart);
                mipLevel++;
            }
        }

        System.out.printf("-----------------\n");
        System.out.printf("Encode type: %s\n", texcCompressionType.toString());

        mipLevel = 0;

        List<byte[]> encodedMips = new ArrayList<>();
        for (long image : images)
        {
            byte[]  uncompressed = TexcLibraryJni.GetData(image);
            int     image_width  = TexcLibraryJni.GetWidth(image);
            int     image_height = TexcLibraryJni.GetHeight(image);

            System.out.printf("  -----------------\n");
            System.out.printf("  Encode mip level: %d  %d x %d  %d bytes\n", mipLevel, image_width, image_height, uncompressed.length);

            byte[] encoded = null;
            if (Texc.CompressionType.CT_BASIS_UASTC == texcCompressionType) {
                System.out.printf("    -----------------\n");
                System.out.printf("    BasisUEncode\n");

                Texc.BasisUEncodeSettings settings = new Texc.BasisUEncodeSettings();
                settings.path = path;
                settings.width = image_width;
                settings.height = image_height;

                settings.pixelFormat = pixelFormat;
                settings.outPixelFormat = outPixelFormat;
                settings.colorSpace = colorSpace;

                settings.numThreads = maxThreads;
                settings.debug = false;

                // CL_BEST
                settings.rdo_uastc = true;
                settings.pack_uastc_flags = 0;
                settings.rdo_uastc_dict_size = 4096;
                settings.rdo_uastc_quality_scalar = 3.0f;

                DebugPrintObject(settings, 1);

                settings.data = uncompressed;

                timeStart = System.currentTimeMillis();

                encoded = TexcLibraryJni.BasisUEncode(settings);
                if (encoded == null || encoded.length == 0) {
                    throw new Exception("Could not encode");
                }

                timeEnd = System.currentTimeMillis();

            }
            else if (Texc.CompressionType.CT_ASTC == texcCompressionType) {
                System.out.printf("    -----------------\n");
                System.out.printf("    ASTCEncode\n");

                Texc.ASTCEncodeSettings settings = new Texc.ASTCEncodeSettings();
                settings.path = path;
                settings.width = image_width;
                settings.height = image_height;

                settings.pixelFormat = pixelFormat;
                //settings.outPixelFormat = outPixelFormat;
                settings.colorSpace = colorSpace;

                // settings.numThreads = maxThreads;
                // settings.debug = false;

                DebugPrintObject(settings, 1);

                settings.data = uncompressed;

                timeStart = System.currentTimeMillis();

                encoded = TexcLibraryJni.ASTCEncode(settings);
                if (encoded == null || encoded.length == 0) {
                    throw new Exception("Could not encode");
                }

                timeEnd = System.currentTimeMillis();
            }
            else if (Texc.CompressionType.CT_DEFAULT == texcCompressionType) {
                System.out.printf("    -----------------\n");
                System.out.printf("    DefaultEncode\n");

                Texc.DefaultEncodeSettings settings = new Texc.DefaultEncodeSettings();
                settings.path = path;
                settings.width = image_width;
                settings.height = image_height;

                settings.pixelFormat = pixelFormat;
                settings.outPixelFormat = outPixelFormat;
                settings.colorSpace = colorSpace;

                settings.numThreads = maxThreads;
                settings.debug = false;

                DebugPrintObject(settings, 1);

                settings.data = uncompressed;

                timeStart = System.currentTimeMillis();

                encoded = TexcLibraryJni.DefaultEncode(settings);
                if (encoded == null || encoded.length == 0) {
                    throw new Exception("Could not encode");
                }

                timeEnd = System.currentTimeMillis();
            }

            encodedMips.add(encoded);

            System.out.printf("Encode mip level %d: %d bytes took %d ms\n", mipLevel, encoded.length, timeEnd - timeStart);
            mipLevel++;

        }

        System.out.printf("-----------------\n");
        System.out.printf("Destroying images\n");
        for (long image : images)
        {
            TexcLibraryJni.DestroyImage(image);
        }

        {
            System.out.printf("-----------------\n");
            System.out.printf("CompressBuffer\n");
            timeStart = System.currentTimeMillis();

            // Just for testing the compress function
            Texc.Buffer buffer = TexcLibraryJni.CompressBuffer(bytes);

            timeEnd = System.currentTimeMillis();
            System.out.printf("CompressBuffer of %d bytes took %d ms\n", bytes.length, timeEnd - timeStart);
            System.out.printf("  uncompressed size: %d\n", bytes.length);
            System.out.printf("    compressed size: %d\n", buffer.data.length);
            System.out.printf("      is compressed: %b\n", buffer.isCompressed);
        }

        System.out.printf("-----------------\n");
        System.out.printf("Done\n");
    }
}
