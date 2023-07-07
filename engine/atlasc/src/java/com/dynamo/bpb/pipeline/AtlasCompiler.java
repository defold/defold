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

    public static class AtlasException extends Exception {
        public AtlasException(String errorMessage) {
            super(errorMessage);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////


    static public enum PackingAlgorithm {
        PA_TILEPACK_AUTO,       // The default (tile)
        PA_TILEPACK_TILE,
        PA_TILEPACK_CONVEXHULL,
        PA_BINPACK_SKYLINE_BL,
    };

    static public class Vec2i {
        int x, y;

        public Vec2i() {}
        public Vec2i(Vec2i other)
        {
            this.x = other.x;
            this.y = other.y;
        }
    };

    static public class Vec2f {
        float x, y;

        public Vec2f() {}
        public Vec2f(Vec2f other)
        {
            this.x = other.x;
            this.y = other.y;
        }
    };

    static public class Rect {
        Vec2i pos;
        Vec2i size;

        public Rect() {}
        public Rect(Rect other)
        {
            this.pos = other.pos;
            this.size = other.size;
        }
    };

    // Input format
    static public class SourceImage
    {
        String  path; // The source path
        byte[]  data; // The texels
        Vec2i   size; // The dimensions in texels
        int     numChannels;
    };

    // Output format
    static public class PackedImage {
        Vec2i pos;
        Vec2i size;

        public PackedImage() {}
        public PackedImage(PackedImage other)
        {
            this.pos = other.pos;
            this.size = other.size;
        }
    };

    // Output format
    static public class AtlasPage {
        int                 index;
        List<PackedImage>   images;
    };

    static public class Atlas {
        List<AtlasPage>     pages;
    };

    static public class Options {
        PackingAlgorithm    algorithm;  // default: PA_TILEPACK_AUTO
        int                 pageSize;   // The max size in texels. default: 0 means all images are stored in the same atlas

        // general packer options
        boolean             packerNoRotate;

        // tile packer options
        int                 tilePackerTileSize;   // The size in texels. Default 16
        int                 tilePackerPadding;    // Internal padding for each image. Default 1
        int                 tIlePackerAlphaThreshold;       // Values below or equal to this threshold are considered transparent. (range 0-255)

        // bin packer options (currently none)
    };

    // public static Scene LoadFromBuffer(Options options, String path, byte[] bytes, DataResolver data_resolver)
    // {
    //     return AtlasCompiler.LoadFromBufferInternal(path, bytes, data_resolver);
    // }

    // ////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: AtlasCompiler.class <model_file>\n");
        System.out.printf("\n");
    }

    private static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    public static void DebugPrintVec2i(Vec2i v, int indent) {
        PrintIndent(indent);
        System.out.printf("v: %d, %d\n", v.x, v.y);
    }

    public static void DebugPrintVec2f(Vec2f v, int indent) {
        PrintIndent(indent);
        System.out.printf("v: %f, %f\n", v.x, v.y);
    }

    public static void DebugPrintRect(Rect r, int indent) {
        PrintIndent(indent);
        System.out.printf("rect: x/y: %f, %f,  w/h: %f, %f\n", r.pos.x, r.pos.y, r.size.x, r.size.y);
    }

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

        // String path = args[0];       // name.glb/.gltf
        // long timeStart = System.currentTimeMillis();

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
