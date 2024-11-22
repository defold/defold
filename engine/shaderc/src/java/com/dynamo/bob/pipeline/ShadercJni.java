
package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class ShadercJni {
    static final String LIBRARY_NAME = "shaderc_shared";

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
                ClassLoader clsloader = ShadercJni.class.getClassLoader();
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

    public static native long CreateShaderContext(byte[] buffer);

    public static native Shaderc.ShaderReflection GetReflection(long context);

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

    /*
    public static class ShaderResource {
        public String name;
        public long nameHash = 0;
        public String instanceName;
        public long instanceNameHash = 0;
        public ResourceType type;
        public byte location = 0;
        public byte binding = 0;
        public byte set = 0;
    };
    public static class ShaderReflection {
        public ShaderResource[] inputs;
        public ShaderResource[] outputs;
        public ShaderResource[] uniformBuffers;
        public ShaderResource[] textures;
        public ResourceTypeInfo[] types;
    };
    */

    private static void printResourceArray(Shaderc.ShaderResource resources[], String label) {
        if (resources != null) {
            System.out.println("Num " + label + ": " + resources.length);
            for (Shaderc.ShaderResource res : resources) {
                System.out.println("name:     " + res.name);
                System.out.println("location: " + res.location);
                System.out.println("binding:  " + res.binding);
                System.out.println("set:      " + res.set);
            }
        }
    }

    private static void Usage() {
        System.out.println("Usage: ShadercJni <path_to_spv> [<path_to_output> <version>]");
    }

    private static void printReflection(long context) {
        Shaderc.ShaderReflection reflection = GetReflection(context);
        printResourceArray(reflection.inputs, "inputs");
        printResourceArray(reflection.outputs, "outputs");
        printResourceArray(reflection.uniformBuffers, "uniformBuffers");
        printResourceArray(reflection.textures, "textures");
    }

    private static void crossCompile(long context, int version) {
        // TODO
    }

    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        String pathSpv = args[0];
        String pathOut = null;
        int version    = 330;

        if (args.length > 1) {
            pathOut = args[1];
        }
        if (args.length > 2) {
            version = Integer.parseInt(args[2]);
        }

        System.out.println("Printing reflectin for: " + pathSpv);

        byte[] spvBytes = ReadFile(pathSpv);
        long ptr = CreateShaderContext(spvBytes);

        if (pathOut == null) {
            printReflection(ptr);
        } else {
            crossCompile(ptr, version);
        }
    }
}
