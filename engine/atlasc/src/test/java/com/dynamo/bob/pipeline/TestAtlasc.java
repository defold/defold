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
import java.util.List;
import java.util.ArrayList;
import java.lang.reflect.Method;

public class TestAtlasc
{
    static final String LIBRARY_NAME = "atlasc_shared";

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

    // public static native Testapi.Vec2i TestCreateVec2i();

    @Test
    public void testConstants() {
        assertEquals(0, Atlasc.PackingAlgorithm.PA_TILEPACK_AUTO.getValue());
    }

    @Test
    public void test_NoImages() {
        System.out.println("*****************************************************");
        System.out.println("Expected error ->");
        Atlasc.Options options = new Atlasc.Options();
        Atlasc.SourceImage[] images = new Atlasc.SourceImage[0];
        Atlasc.Atlas atlas = AtlasCompiler.CreateAtlas(options, images);
        assertTrue(atlas == null);
        System.out.println("<- Expected error");
        System.out.println("*****************************************************");
    }

    private Atlasc.SourceImage[] loadImages(File folder) {
        if (!folder.exists()) {
            System.out.printf("Folder does not exist: %s\n", folder);
            return null;
        }

        List<Atlasc.SourceImage> images = new ArrayList<>();
        for (File file : folder.listFiles())
        {
            Atlasc.SourceImage image = AtlasCompiler.LoadImage(file.getAbsolutePath());
            images.add(image);
        }
        return images.toArray(new Atlasc.SourceImage[0]);
    }

    // TODO: public void test_TilePackWithNoData() {

    @Test
    public void test_TilePack() {
        Atlasc.Options options = AtlasCompiler.GetDefaultOptions();
        options.algorithm = Atlasc.PackingAlgorithm.PA_TILEPACK_TILE;
        Atlasc.SourceImage[] images = loadImages(new File("./src/test/data/atlas01"));
        Atlasc.Atlas atlas = AtlasCompiler.CreateAtlas(options, images);
        assertFalse(atlas == null);

        assertEquals(1, atlas.pages.length);
        assertEquals(512, atlas.pages[0].dimensions.width);
        assertEquals(256, atlas.pages[0].dimensions.height);
        assertEquals(7, atlas.pages[0].images.length);
    }

    // ----------------------------------------------------

    public static void main(String[] args) {
        JUnitCore junit = new JUnitCore();
        junit.addListener(new TextListener(System.out));
        junit.run(TestAtlasc.class);
    }
}
