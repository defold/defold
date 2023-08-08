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
    public static native Testapi.Misc TestCreateMisc();

    public static native Testapi.Recti TestDuplicateRecti(Testapi.Recti rect);
    public static native Testapi.Arrays TestDuplicateArrays(Testapi.Arrays arrays);

    // public static native void TestException(String message);

    @Test
    public void testConstants() {
        assertEquals(10, Testapi.CONSTANT_INT);
        assertEquals(2.5f, Testapi.CONSTANT_FLOAT, 0.0001f);
        assertEquals(1.5f, Testapi.CONSTANT_DOUBLE, 0.0001);
    }

    @Test
    public void testEnumDefault() {
        assertEquals(0, Testapi.TestEnumDefault.TED_VALUE_A.getValue());
        assertEquals(1, Testapi.TestEnumDefault.TED_VALUE_B.getValue());
        assertEquals(2, Testapi.TestEnumDefault.TED_VALUE_C.getValue());
    }

    @Test
    public void testEnum() {
        assertEquals(2, Testapi.TestEnum.TE_VALUE_A.getValue());
        assertEquals(3, Testapi.TestEnum.TE_VALUE_B.getValue());
        assertEquals(-4, Testapi.TestEnum.TE_VALUE_C.getValue());
        assertEquals(5, Testapi.TestEnum.TE_VALUE_D.getValue());
        assertEquals(10, Testapi.TestEnum.TE_VALUE_E.getValue());
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

    // C to Java

    @Test
    public void testJniVec2i() {
        Testapi.Vec2i vec = TestCreateVec2i();
        assertEquals(1, vec.x);
        assertEquals(2, vec.y);
    }

    @Test
    public void testJniRecti() {
        Testapi.Recti rect = TestCreateRecti();
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

    @Test
    public void testJniMisc() {
        Testapi.Misc misc = TestCreateMisc();
        assertEquals(Testapi.TestEnum.TE_VALUE_B, misc.testEnum);
        assertEquals("Hello World!", misc.string);
    }

    // ----------------------------------------------------

    // Java to C

    @Test
    public void testJ2C_Recti() {
        Testapi.Recti rect = TestCreateRecti();
        Testapi.Recti rect2 = TestDuplicateRecti(rect);

        assertEquals(-2, rect.min.x);
        assertEquals(-3, rect.min.y);
        assertEquals(4, rect.max.x);
        assertEquals(5, rect.max.y);
        assertEquals(-1, rect2.min.x);
        assertEquals(-2, rect2.min.y);
        assertEquals(5, rect2.max.x);
        assertEquals(6, rect2.max.y);
    }

    @Test
    public void testJ2C_Arrays() {
        Testapi.Arrays arrays = TestCreateArrays();
        assertEquals(4, arrays.data.length);
        assertEquals(5, arrays.data2.length);
        assertEquals(3, arrays.rects.length);
        assertEquals(3, arrays.rects2.length);

        Testapi.Arrays arrays2 = TestDuplicateArrays(arrays);


        byte[] data1 = new byte[] { 2,3,5,9 };
        assertEquals(4, arrays2.data.length);
        assertArrayEquals( data1, arrays2.data );

        byte[] data2 = new byte[] { 3, 5, 9, 17, 33 };
        assertEquals(5, arrays2.data2.length);
        assertArrayEquals( data2, arrays2.data2 );

        assertEquals(3, arrays2.rects.length);
        for (int i = 0; i < 3; ++i) {
            assertEquals(i*4+1 + 1, arrays2.rects[i].min.x);
            assertEquals(i*4+2 + 1, arrays2.rects[i].min.y);
            assertEquals(i*4+3 + 1, arrays2.rects[i].max.x);
            assertEquals(i*4+4 + 1, arrays2.rects[i].max.y);
        }

        assertEquals(3, arrays2.rects2.length);
        for (int i = 0; i < 3; ++i) {
            assertEquals(i*4+1 + 1, arrays2.rects2[i].min.x);
            assertEquals(i*4+2 + 1, arrays2.rects2[i].min.y);
            assertEquals(i*4+3 + 1, arrays2.rects2[i].max.x);
            assertEquals(i*4+4 + 1, arrays2.rects2[i].max.y);
        }
    }

    // ----------------------------------------------------

    public static void main(String[] args) {
        JUnitCore junit = new JUnitCore();
        junit.addListener(new TextListener(System.out));
        junit.run(JniTest.class);
    }
}
