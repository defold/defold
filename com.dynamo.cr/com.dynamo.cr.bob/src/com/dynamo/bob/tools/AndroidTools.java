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

package com.dynamo.bob.tools;

import com.dynamo.bob.bundle.AndroidBundler;


import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.TimeProfiler;

import com.dynamo.bob.tools.ToolsHelper;



public class AndroidTools {
    private static Logger logger = Logger.getLogger(AndroidTools.class.getName());

    private static boolean initialized = false;

    /**
     * Execute a command.
     * Sets LD_LIBRARY_PATH on Linux before running the command.
     * TODO: Could this be done in com.dynamo.bob.util.Exec instead?
     * @param args List of arguments
     * @return The result
     */
    public static Result exec(List<String> args) throws IOException {
        init();
        logger.info("exec: " + String.join(" ", args));
        Map<String, String> env = new HashMap<String, String>();
        if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.Arm64Linux) {
            env.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
        }
        Result res = Exec.execResultWithEnvironment(env, args);
        String stdout = new String(res.stdOutErr, StandardCharsets.UTF_8);
        if (res.ret != 0) {
            throw new IOException(stdout);
        }
        else {
            logger.info("result: %s", stdout);
        }
        return res;
    }

    public static Result exec(String... args) throws IOException {
        return exec(Arrays.asList(args));
    }

    private static String getAapt2Name() {
        if (Platform.getHostPlatform() == Platform.X86_64Win32)
            return "aapt2.exe";
        return "aapt2";
    }

    private static synchronized void init() {
        TimeProfiler.start("Init Android");
        if (!initialized)
        {
            Bob.init();
            File rootFolder = Bob.getRootFolder();
            try {
                // Android SDK aapt is dynamically linked against libc++.so, we need to extract it so that
                // aapt will find it later when AndroidBundler is run.
                String libcFilename = Platform.getHostPlatform().getLibPrefix() + "c++" + Platform.getHostPlatform().getLibSuffix();
                URL libcUrl = Bob.class.getResource("/lib/" + Platform.getHostPlatform().getPair() + "/" + libcFilename);
                if (libcUrl != null) {
                    File f = new File(rootFolder, Platform.getHostPlatform().getPair() + "/lib/" + libcFilename);
                    Bob.atomicCopy(libcUrl, f, false);
                }

                // NOTE: android.jar and classes.dex are only available in "full bob", i.e. from CI
                URL androidJar = Bob.class.getResource("/lib/android.jar");
                if (androidJar != null) {
                    File f = new File(rootFolder, "lib/android.jar");
                    Bob.atomicCopy(androidJar, f, false);
                }
                URL classesDex = Bob.class.getResource("/lib/classes.dex");
                if (classesDex != null) {
                    File f = new File(rootFolder, "lib/classes.dex");
                    Bob.atomicCopy(classesDex, f, false);
                }

                Platform hostPlatform = Platform.getHostPlatform();
                String aapt2 = Bob.getExe(hostPlatform, "aapt2");
                if (aapt2 == null) {
                    // Make sure it's extracted once
                    File bundletool = new File(Bob.getLibExecPath("bundletool-all.jar"));

                    // Find the file to extract from the bundletool
                    {
                        String platformName = "macos";
                        String suffix = "";
                        if (hostPlatform == Platform.X86_64Win32)
                        {
                            platformName = "windows";
                            suffix = ".exe";
                        }
                        else if (hostPlatform == Platform.X86_64Linux)
                        {
                            platformName = "linux";
                        }

                        File aapt2File = new File(bundletool.getParent(), getAapt2Name());
                        if (!aapt2File.exists())
                        {
                            ToolsHelper.extractFile(bundletool, platformName + "/" + getAapt2Name(), aapt2File);
                            aapt2File.setExecutable(true);
                        }
                    }
                }

            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            initialized = true;
        }
        TimeProfiler.stop();
    }


    /**
     * Run the Java jarsigner tool
     * https://docs.oracle.com/en/java/javase/21/docs/specs/man/jarsigner.html
     * @param jarsignerargs List of jarsigner arguments
     * @return The result of running jarsigner
     */
    public static Result jarsigner(List<String> jarsignerargs) throws IOException {
        init();

        List<String> args = new ArrayList<>();
        args.add(ToolsHelper.getJavaBinFile("jarsigner"));
        args.addAll(jarsignerargs);

        return exec(args);
    }

    /**
     * Run the Android Asset Packaging Tool with a set of arguments
     * https://developer.android.com/tools/aapt2
     * @param aapt2args List of aapt2 arguments
     * @return The result of running aapt2
     */
    public static Result aapt2(List<String> aapt2args) throws IOException {
        init();

        String aapt2 = Bob.getExe(Platform.getHostPlatform(), "aapt2");
        if (aapt2 == null) {
            aapt2 = Bob.getLibExecPath(getAapt2Name());
        }

        List<String> args = new ArrayList<>();
        args.add(aapt2);
        args.addAll(aapt2args);

        return exec(args);
    }

    /**
     * Run the Android bundle tool
     * https://developer.android.com/tools/bundletool
     * @param bundletoolargs List of bundletool arguments
     * @return The result of running bundletool
     */
    public static Result bundletool(List<String> bundletoolargs) throws IOException {
        init();

        List<String> args = new ArrayList<>();
        args.add(ToolsHelper.getJavaBinFile("java")); args.add("-jar");
        args.add(Bob.getLibExecPath("bundletool-all.jar"));
        args.addAll(bundletoolargs);

        return exec(args);
    }


    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: AndroidTools <aapt2|bundletool|jarsigner> <arg1..n>");
            return;
        }

        // create list of args, starting from the second value (first is the command to run)
        List<String> argslist = new ArrayList<>();
        for (int i = 1; i < args.length; i++) {
            argslist.add(args[i]);
        }

        final String command = args[0];
        switch (command) {
            case "aapt2":
                aapt2(argslist);
                break;
            case "bundletool":
                bundletool(argslist);
                break;
            case "jarsigner":
                jarsigner(argslist);
                break;
            default:
                System.out.println("Unknown command: " + command);
        }
    }
}