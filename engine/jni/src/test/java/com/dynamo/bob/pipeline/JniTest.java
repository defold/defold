package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertArrayEquals;

import org.junit.Test;
import org.junit.runner.JUnitCore;
import org.junit.internal.TextListener;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class JniTest
{
    static final String LIBRARY_NAME = "test_jni_shared";

    static {
        // Class<?> clsbob = null;

        // try {
        //     ClassLoader clsloader = ClassLoader.getSystemClassLoader();
        //     clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
        // } catch (Exception e) {
        //     System.out.printf("Didn't find Bob class in default system class loader: %s\n", e);
        // }

        // if (clsbob == null) {
        //     try {
        //         // ClassLoader.getSystemClassLoader() doesn't work with junit
        //         ClassLoader clsloader = ModelImporter.class.getClassLoader();
        //         clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
        //     } catch (Exception e) {
        //         System.out.printf("Didn't find Bob class in default test class loader: %s\n", e);
        //     }
        // }

        // if (clsbob != null)
        // {
        //     try {
        //         Method getSharedLib = clsbob.getMethod("getSharedLib", String.class);
        //         Method addToPaths = clsbob.getMethod("addToPaths", String.class);
        //         File lib = (File)getSharedLib.invoke(null, LIBRARY_NAME);
        //         System.load(lib.getAbsolutePath());
        //     } catch (Exception e) {
        //         System.err.printf("Failed to find functions in Bob: %s\n", e);
        //         System.exit(1);
        //     }
        // }
        // else {
            try {
                System.out.printf("Fallback to regular System.loadLibrary(%s)\n", LIBRARY_NAME);
                System.loadLibrary(LIBRARY_NAME); // Requires the java.library.path to be set
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                System.exit(1);
            }
        // }
    }

    // The suffix of the path dictates which loader it will use
    //public static native Scene LoadFromBufferInternal(String path, byte[] buffer, Object data_resolver);
    public static native Testapi.Vec2i TestCreateVec2i();
    public static native Testapi.Recti TestCreateRecti();
    public static native Testapi.Arrays TestCreateArrays();
    // public static native void TestException(String message);

    @Test
    public void testConstants() {
        assertEquals(10, Testapi.CONSTANT_INT);
        assertEquals(2.5f, Testapi.CONSTANT_FLOAT, 0.0001f);
        assertEquals(1.5f, Testapi.CONSTANT_DOUBLE, 0.0001);
    }

    @Test
    public void testEnum() {
        assertEquals(1, Testapi.TestEnum.VALUE_A.getValue());
        assertEquals(2, Testapi.TestEnum.VALUE_B.getValue());
        assertEquals(4, Testapi.TestEnum.VALUE_C.getValue());
    }

    @Test
    public void testVec2i() {
        Testapi.Vec2i vec = new Testapi.Vec2i();
        assertEquals(0, vec.x);
        assertEquals(0, vec.y);
    }

    @Test
    public void testRecti() {
        Testapi.Recti rect = new Testapi.Recti();
        assertEquals(null, rect.min);
        assertEquals(null, rect.max);
    }

    // ----------------------------------------------------

    // Jni

    @Test
    public void testJniVec2i() {
        Testapi.Vec2i vec = TestCreateVec2i(); // (1, 2)
        assertEquals(1, vec.x);
        assertEquals(2, vec.y);
    }

    @Test
    public void testJniRecti() {
        Testapi.Recti rect = TestCreateRecti(); // (1, 2)
        assertEquals(-2, rect.min.x);
        assertEquals(-3, rect.min.y);
        assertEquals(4, rect.max.x);
        assertEquals(5, rect.max.y);
    }

    @Test
    public void testJniArrays() {
        Testapi.Arrays arrays = TestCreateArrays();

        byte[] data1 = new byte[] { 1,2,4,8 };
        assertEquals(4, arrays.data.length);
        assertArrayEquals( data1, arrays.data );

        byte[] data2 = new byte[] { 2, 4, 8, 16, 32 };
        assertEquals(5, arrays.data2.length);
        assertArrayEquals( data2, arrays.data2 );

        assertEquals(3, arrays.rects.length);
        for (int i = 0; i < 3; ++i) {
            assertEquals(i*4+1, arrays.rects[i].min.x);
            assertEquals(i*4+2, arrays.rects[i].min.y);
            assertEquals(i*4+3, arrays.rects[i].max.x);
            assertEquals(i*4+4, arrays.rects[i].max.y);
        }

        assertEquals(3, arrays.rects2.length);
        for (int i = 0; i < 3; ++i) {
            assertEquals(i*4+1, arrays.rects2[i].min.x);
            assertEquals(i*4+2, arrays.rects2[i].min.y);
            assertEquals(i*4+3, arrays.rects2[i].max.x);
            assertEquals(i*4+4, arrays.rects2[i].max.y);
        }
    }

    // ----------------------------------------------------

    public static void main(String[] args) {
        JUnitCore junit = new JUnitCore();
        junit.addListener(new TextListener(System.out));
        junit.run(JniTest.class);
    }
}
