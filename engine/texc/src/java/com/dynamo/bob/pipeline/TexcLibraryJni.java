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

    public static native long CreateTexture(String name, int width, int height, int pixelFormat, int colorSpace, int compressionType, byte[] data);
    public static native void DestroyTexture(long texture);

    public static native Texc.Header GetHeader(long texture);
    public static native int GetDataSizeCompressed(long texture, int mipMap);
    public static native int GetDataSizeUncompressed(long texture, int mipMap);
    public static native byte[] GetData(long texture);
    public static native int GetCompressionFlags(long texture);

    public static native boolean Resize(long texture, int width, int height);
    public static native boolean PreMultiplyAlpha(long texture);
    public static native boolean GenMipMaps(long texture);
    public static native boolean Flip(long texture, int flipAxis);

    public static native boolean Encode(long texture, int pixelFormat, int colorSpace, int compressionLevel, int compressionType, boolean mipmaps, int num_threads);

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

        String path = args[0];
        System.out.printf("Loading '%s'\n", path);

        long timeStart = System.currentTimeMillis();

        BufferedImage image = null;
        try {
            image = ImageIO.read(new FileInputStream(path));
        } catch (Exception e) {
            System.out.printf("Failed to load image '%s': %s\n", path, e);
            return;
        }

        ByteBuffer byteBuffer = getByteBuffer(image);
        byte[] bytes = byteBuffer.array();
        long timeEnd = System.currentTimeMillis();

        System.out.printf("Loaded '%s' in %d ms\n", path, timeEnd - timeStart);

        timeStart = System.currentTimeMillis();
        long texture = TexcLibraryJni.CreateTexture(path, image.getWidth(), image.getHeight(),
                                                        Texc.PixelFormat.PF_A8B8G8R8.getValue(),
                                                        Texc.ColorSpace.CS_SRGB.getValue(),
                                                        Texc.CompressionType.CT_DEFAULT.getValue(),
                                                        bytes);

        timeEnd = System.currentTimeMillis();
        if (texture == 0) {
            System.out.printf("Failed to create texture for '%s'\n", path);
            return;
        }

        System.out.printf("Created texture in %d ms\n", timeEnd - timeStart);

        for (int i = 0; i < args.length; ++i)
        {
            // if (args[i].equalsIgnoreCase("--test-exception"))
            // {
            //     TexcLibraryJni.TestException("Testing exception: " + args[i+1]);
            //     return; // exit code 0
            // }
        }

        Texc.Header header = TexcLibraryJni.GetHeader(texture);
        System.out.printf("-----------------\nHeader\n");
        DebugPrintObject(header, 1);

        int width = image.getWidth();
        int height = image.getHeight();

        int maxThreads = 4;
        boolean premulAlpha = true;
        boolean generateMipMaps = true;
        Texc.PixelFormat pixelFormat = Texc.PixelFormat.PF_R8G8B8A8;
        Texc.CompressionLevel texcCompressionLevel = Texc.CompressionLevel.CL_BEST;
        //Texc.CompressionType texcCompressionType = Texc.CompressionType.CT_DEFAULT;
        Texc.CompressionType texcCompressionType = Texc.CompressionType.CT_BASIS_UASTC;

        // Premultiply before scale so filtering cannot introduce colour artefacts.
        if (premulAlpha && !ColorModel.getRGBdefault().isAlphaPremultiplied()) {
            System.out.printf("-----------------\n");
            System.out.printf("PreMultiplyAlpha\n");
            timeStart = System.currentTimeMillis();
            if (!TexcLibraryJni.PreMultiplyAlpha(texture)) {
                throw new Exception("could not premultiply alpha");
            }
            timeEnd = System.currentTimeMillis();
            System.out.printf("PreMultiplyAlpha took %d ms\n", timeEnd - timeStart);
        }

        int newWidth = width / 2;
        int newHeight = height / 2;

        if (width != newWidth || height != newHeight) {
            System.out.printf("-----------------\n");
            System.out.printf("Resize to %dx%d from %dx%d\n", newWidth, newHeight, width, height);
            timeStart = System.currentTimeMillis();
            if (!TexcLibraryJni.Resize(texture, newWidth, newHeight)) {
                throw new Exception("could not resize texture to POT");
            }
            timeEnd = System.currentTimeMillis();
            System.out.printf("Resize took %d ms\n", timeEnd - timeStart);
        }

        // Loop over all axis that should be flipped.
        EnumSet<Texc.FlipAxis> flipAxis = EnumSet.of(Texc.FlipAxis.FLIP_AXIS_Y);
        for (Texc.FlipAxis flip : flipAxis) {
            System.out.printf("-----------------\n");
            System.out.printf("Flip %s\n", flip.toString());
            timeStart = System.currentTimeMillis();
            if (!TexcLibraryJni.Flip(texture, flip.getValue())) {
                throw new Exception("could not flip on " + flip.toString());
            }
            timeEnd = System.currentTimeMillis();
            System.out.printf("Flip took %d ms\n", timeEnd - timeStart);
        }


        if (generateMipMaps) {
            System.out.printf("-----------------\n");
            System.out.printf("GenMipMaps\n");
            timeStart = System.currentTimeMillis();
            if (!TexcLibraryJni.GenMipMaps(texture)) {
                throw new Exception("could not generate mip-maps");
            }
            timeEnd = System.currentTimeMillis();
            System.out.printf("GenMipMaps took %d ms\n", timeEnd - timeStart);
        }

        {
            System.out.printf("-----------------\n");
            System.out.printf("Encode\n");
            timeStart = System.currentTimeMillis();
            if (!TexcLibraryJni.Encode(texture, pixelFormat.getValue(),
                                                Texc.ColorSpace.CS_SRGB.getValue(),
                                                texcCompressionLevel.getValue(),
                                                texcCompressionType.getValue(),
                                                generateMipMaps, maxThreads)) {
                throw new Exception("could not encode");
            }
            timeEnd = System.currentTimeMillis();
            System.out.printf("Encode took %d ms\n", timeEnd - timeStart);
        }

        {
            System.out.printf("-----------------\n");
            System.out.printf("GetData\n");
            timeStart = System.currentTimeMillis();

            byte[] data = TexcLibraryJni.GetData(texture);

            timeEnd = System.currentTimeMillis();
            System.out.printf("GetData of %d bytes took %d ms\n", data.length, timeEnd - timeStart);

            boolean texcBasisCompression = false;

            // If we're writing a .basis file, we don't actually store each mip map separately
            // In this case, we pretend that there's only one mip level
            if (texcCompressionType == Texc.CompressionType.CT_BASIS_UASTC ||
                texcCompressionType == Texc.CompressionType.CT_BASIS_ETC1S )
            {
                generateMipMaps = false;
                texcBasisCompression = true;
            }

            System.out.printf("-----------------\n");
            System.out.printf("Get mip maps\n");

            int w = newWidth;
            int h = newHeight;
            int offset = 0;
            int mipMap = 0;
            while (w != 0 || h != 0) {
                w = Math.max(w, 1);
                h = Math.max(h, 1);

                System.out.printf("Mip: %d:  offset %d\n", mipMap, offset);

                int size_uncompressed = TexcLibraryJni.GetDataSizeUncompressed(texture, mipMap);
                int size_compressed = size_uncompressed;

                // For basis the GetDataSizeCompressed and GetDataSizeUncompressed will always return 0,
                // so we use this hack / workaround to calculate offsets in the engine..
                if (texcBasisCompression)
                {
                    size_uncompressed   = data.length;
                    size_compressed     = data.length;
                }
                else
                {
                    size_compressed = TexcLibraryJni.GetDataSizeCompressed(texture, mipMap);
                }

                size_compressed = size_compressed != 0 ? size_compressed : size_uncompressed;

                System.out.printf("  uncompressed size: %d\n", size_uncompressed);
                System.out.printf("    compressed size: %d\n", size_compressed);

                offset += size_compressed;
                w >>= 1;
                h >>= 1;
                mipMap += 1;

                if (!generateMipMaps) // Run section only once for non-mipmaps
                    break;
            }

            {
                System.out.printf("-----------------\n");
                System.out.printf("CompressBuffer\n");
                timeStart = System.currentTimeMillis();

                // Just for testing the compress function
                Texc.Buffer buffer = TexcLibraryJni.CompressBuffer(data);

                timeEnd = System.currentTimeMillis();
                System.out.printf("CompressBuffer of %d bytes took %d ms\n", data.length, timeEnd - timeStart);
                System.out.printf("  uncompressed size: %d\n", data.length);
                System.out.printf("    compressed size: %d\n", buffer.data.length);
                System.out.printf("      is compressed: %b\n", buffer.isCompressed);
            }
        }

        TexcLibraryJni.DestroyTexture(texture);

        System.out.printf("-----------------\n");
        System.out.printf("Done\n");
    }
}
