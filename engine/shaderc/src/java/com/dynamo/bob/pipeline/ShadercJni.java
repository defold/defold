// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.


package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import com.dynamo.bob.pipeline.Shaderc;

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

    public static native long NewShaderContext(int stage, byte[] buffer);
    public static native void DeleteShaderContext(long context);

    public static native long NewShaderCompiler(long context, int version);
    public static native void DeleteShaderCompiler(long compiler);

    public static native Shaderc.ShaderReflection GetReflection(long context);

    public static native void SetResourceLocation(long context, long compiler, long nameHash, int location);
    public static native void SetResourceBinding(long context, long compiler, long nameHash, int binding);
    public static native void SetResourceSet(long context, long compiler, long nameHash, int set);
    public static native void SetResourceStageFlags(long context, long nameHash, int stageFlags);

    public static native Shaderc.ShaderCompileResult Compile(long context, long compiler, Shaderc.ShaderCompilerOptions options);
    public static native Shaderc.HLSLRootSignature   HLSLMergeRootSignatures(Shaderc.ShaderCompileResult[] shaders);

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

    private static void printResourceArray(Shaderc.ShaderResource resources[], String label) {
        if (resources != null) {
            System.out.println("---- " + label + " ----");
            System.out.println("Resource-count: " + resources.length);
            int c=0;
            for (Shaderc.ShaderResource res : resources) {
                System.out.println("[" + c++ + "]:");
                System.out.println("  ID:       " + res.id);
                System.out.println("  name:     " + res.name);
                System.out.println("  location: " + res.location);
                System.out.println("  binding:  " + res.binding);
                System.out.println("  set:      " + res.set);
            }
        }
    }

    private static void printResourceTypes(Shaderc.ResourceTypeInfo types[], String label) {
        if (types != null) {
            System.out.println("---- " + label + " ----");
            System.out.println("Resource-count: " + types.length);
            int c=0;
            for (Shaderc.ResourceTypeInfo res : types) {
                System.out.println("[" + c++ + "]:");
                System.out.println("  name: " + res.name);

                int m=0;
                for (Shaderc.ResourceMember member : res.members) {
                    System.out.println("  [" + m++ + "]:");
                    System.out.println("    name:     " + member.name);
                }
            }
        }
    }

    private static void Usage() {
        System.out.println("Usage: ShadercJni <path_to_spv> [<version> <stage=(vs|fs)>]");
    }

    private static void printReflection(long context) {
        Shaderc.ShaderReflection reflection = GetReflection(context);
        printResourceArray(reflection.inputs, "inputs");
        printResourceArray(reflection.outputs, "outputs");
        printResourceArray(reflection.uniformBuffers, "uniformBuffers");
        printResourceArray(reflection.storageBuffers, "storageBuffers");
        printResourceArray(reflection.textures, "textures");
        printResourceTypes(reflection.types, "resourceTypes");
    }

    private static void crossCompile(long context, int version) {
        long compiler = NewShaderCompiler(context, Shaderc.ShaderLanguage.SHADER_LANGUAGE_GLSL.getValue());

        Shaderc.ShaderCompilerOptions options = new Shaderc.ShaderCompilerOptions();
        options.version    = version;
        options.entryPoint = "main";

        Shaderc.ShaderCompileResult res = Compile(context, compiler, options);

        System.out.println("Result:");
        System.out.println(new String(res.data));

        DeleteShaderCompiler(compiler);
    }

    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        String pathSpv            = args[0];
        int version               = -1;
        Shaderc.ShaderStage stage = Shaderc.ShaderStage.SHADER_STAGE_VERTEX;

        if (args.length > 1) {
            version = Integer.parseInt(args[1]);
        }
        if (args.length > 2) {
            if (args[2].equals("vs")) {
                stage = Shaderc.ShaderStage.SHADER_STAGE_VERTEX;
            } else if (args[2].equals("fs")) {
                stage = Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT;
            }
        }

        byte[] spvBytes = ReadFile(pathSpv);
        long context = NewShaderContext(stage.getValue(), spvBytes);

        if (version < 0) {
            System.out.println("Printing reflection for: " + pathSpv);
            printReflection(context);
        } else {
            crossCompile(context, version);
        }

        DeleteShaderContext(context);
    }
}
