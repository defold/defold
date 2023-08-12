//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.List;
import java.util.ArrayList;

public class AtlasCompiler {

    static final String LIBRARY_NAME = "atlasc_shared";

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
                ClassLoader clsloader = AtlasCompiler.class.getClassLoader();
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
            } catch (Exception e) {
                System.err.printf("Failed to find functions in Bob: %s\n", e);
                System.exit(1);
            }
        }
        else {
            try {
                System.out.printf("Fallback to regular System.loadLibrary(%s)\n", LIBRARY_NAME);
                System.loadLibrary(LIBRARY_NAME); // Requires the java.library.path to be set
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                System.exit(1);
            }
        }
    }

    // // The suffix of the path dictates which loader it will use
    // public static native Scene LoadFromBufferInternal(String path, byte[] buffer, Object data_resolver);
    // public static native int AddressOf(Object o);
    // public static native void TestException(String message);

    public static native Atlasc.Options GetDefaultOptions();
    public static native Atlasc.Atlas   CreateAtlas(Atlasc.Options options, Atlasc.SourceImage[] images);
    //public static native void           DestroyAtlas(Atlasc.Atlas atlas);

    public static native Atlasc.SourceImage     LoadImage(String path);
    public static native Atlasc.RenderedPage    RenderPage(Atlasc.AtlasPage page);
    public static native int                    SavePage(String path, Atlasc.AtlasPage page);

    public static class AtlasException extends Exception {
        public AtlasException(String errorMessage) {
            super(errorMessage);
        }
    }

    // ////////////////////////////////////////////////////////////////////////////////

    static public Atlasc.SourceImage[] loadImages(File folder) {
        if (!folder.exists()) {
            System.out.printf("Folder does not exist: %s\n", folder);
            return null;
        }

        List<Atlasc.SourceImage> images = new ArrayList<>();
        for (File file : folder.listFiles())
        {
            if (!file.isFile())
                continue;
            Atlasc.SourceImage image = AtlasCompiler.LoadImage(file.getAbsolutePath());
            images.add(image);
        }
        return images.toArray(new Atlasc.SourceImage[0]);
    }

    // ////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: AtlasCompiler.class <options ...>\n");
        System.out.printf("\n");
    }

    private static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    // public static void DebugPrintVec2i(Vec2i v, int indent) {
    //     PrintIndent(indent);
    //     System.out.printf("v: %d, %d\n", v.x, v.y);
    // }

    // public static void DebugPrintVec2f(Vec2f v, int indent) {
    //     PrintIndent(indent);
    //     System.out.printf("v: %f, %f\n", v.x, v.y);
    // }

    // public static void DebugPrintRect(Rect r, int indent) {
    //     PrintIndent(indent);
    //     System.out.printf("rect: x/y: %f, %f,  w/h: %f, %f\n", r.pos.x, r.pos.y, r.size.x, r.size.y);
    // }

    // private static void DebugPrintTree(Node node, int indent) {
    //     DebugPrintNode(node, indent);

    //     for (Node child : node.children) {
    //         DebugPrintTree(child, indent+1);
    //     }
    // }

    // private static void DebugPrintFloatArray(int indent, String name, float[] arr, int count, int elements)
    // {
    //     if (arr == null)
    //         return;
    //     PrintIndent(indent);
    //     System.out.printf("%s:\t", name);
    //     for (int i = 0; i < count; ++i) {
    //         System.out.printf("(");
    //         for (int e = 0; e < elements; ++e) {
    //             System.out.printf("%f", arr[i*elements + e]);
    //             if (e < (elements-1))
    //                 System.out.printf(", ");
    //         }
    //         System.out.printf(")");
    //         if (i < (count-1))
    //             System.out.printf(", ");
    //     }
    //     System.out.printf("\n");
    // }

    public static byte[] ReadFile(File file) throws IOException
    {
        InputStream inputStream = new FileInputStream(file);
        byte[] bytes = new byte[inputStream.available()];
        inputStream.read(bytes);
        return bytes;
    }

    public static byte[] ReadFile(String path) throws IOException
    {
        return ReadFile(new File(path));
    }


    // Used for testing the atlas generator. Usage:
    //   ./src/com/dynamo/bob/pipeline/test_atlas_generator.sh <folder path>
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        // // TODO: Setup a java test suite for the model importer
        // for (int i = 0; i < args.length; ++i)
        // {
        //     if (args[i].equalsIgnoreCase("--test-exception"))
        //     {
        //         AtlasCompiler.TestException("Testing exception: " + args[i+1]);
        //         return; // exit code 0
        //     }
        // }

        File path = new File(args[0]); // folder

        if (!path.isDirectory()) {
            System.err.printf("'%s'is not a directory.\n", path);
            System.exit(1);
        }

        long timeStart = System.currentTimeMillis();

        Atlasc.Options options = AtlasCompiler.GetDefaultOptions();
        options.algorithm = Atlasc.PackingAlgorithm.PA_TILEPACK_TILE;
        Atlasc.SourceImage[] images = AtlasCompiler.loadImages(path);
        if (images.length == 0)
        {
            System.out.printf("No images found in folder %s\n", path);
            System.exit(1);
        }
        Atlasc.Atlas atlas = AtlasCompiler.CreateAtlas(options, images);

        int page_counter = 0;
        for (Atlasc.AtlasPage page : atlas.pages)
        {
            String outName = String.format("%s_page_%d.png", path.getName(), page_counter);
            File outputFile = new File(path.getParent(), outName);
            if (AtlasCompiler.SavePage(outputFile.getAbsolutePath(), page) == 0)
            {
                System.out.printf("Failed to write %s\n", outputFile);
                break;
            }

            System.out.printf("Wrote %s\n", outputFile);

            page_counter += 1;
        }

        // FileDataResolver buffer_resolver = new FileDataResolver();
        // Scene scene = LoadFromBuffer(new Options(), path, ReadFile(path), buffer_resolver);

        // long timeEnd = System.currentTimeMillis();

        // System.out.printf("Loaded %s %s\n", path, scene!=null ? "ok":"failed");
        // System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        // if (scene == null)
        //     return;

        // System.out.printf("--------------------------------\n");

        // for (Buffer buffer : scene.buffers) {
        //     System.out.printf("Buffer: uri: %s  data: %s\n", buffer.uri, buffer.buffer);
        // }

        // System.out.printf("--------------------------------\n");

        // System.out.printf("Num Materials: %d\n", scene.materials.length);
        // for (Material material : scene.materials)
        // {
        //     DebugPrintMaterial(material, 0);
        // }

        // System.out.printf("--------------------------------\n");

        // System.out.printf("Num Nodes: %d\n", scene.nodes.length);
        // for (Node node : scene.nodes)
        // {
        //     DebugPrintNode(node, 0);
        // }

        // System.out.printf("--------------------------------\n");

        // for (Node root : scene.rootNodes) {
        //     System.out.printf("Root Node: %s\n", root.name);
        //     DebugPrintTree(root, 0);
        // }

        // System.out.printf("--------------------------------\n");

        // System.out.printf("Num Skins: %d\n", scene.skins.length);
        // for (Skin skin : scene.skins)
        // {
        //     DebugPrintSkin(skin, 0);
        // }

        // System.out.printf("--------------------------------\n");

        // System.out.printf("Num Models: %d\n", scene.models.length);
        // for (Model model : scene.models) {
        //     DebugPrintModel(model, 0);
        // }

        // System.out.printf("--------------------------------\n");

        // System.out.printf("Num Animations: %d\n", scene.animations.length);
        // for (Animation animation : scene.animations) {
        //     DebugPrintAnimation(animation, 0);
        // }

        // System.out.printf("--------------------------------\n");
    }
}
