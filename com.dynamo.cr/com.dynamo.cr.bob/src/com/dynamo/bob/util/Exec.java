// Copyright 2020-2024 The Defold Foundation
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

package com.dynamo.bob.util;
import com.dynamo.bob.Platform;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.logging.Logger;

public class Exec {

    private static String verbosity = System.getenv("DM_BOB_VERBOSE");
    private static Logger logger = Logger.getLogger(Exec.class.getCanonicalName());

    private static int getVerbosity() {
        if (verbosity == null)
            return 0;
        try {
            return Integer.parseInt(verbosity);
        } catch (NumberFormatException nfe) {
            return 0;
        }
    }

    private static void addJavaBinPath(ProcessBuilder pb) {
        Platform platform = Platform.getHostPlatform();
        if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32) {
            // On Windows `exe` files often require vcruntime140_1.dll and vcruntime140.dll
            // these files are available in jdk/bin folder
            // see https://github.com/defold/defold/issues/8277#issuecomment-1836823183
            String path = System.getenv("PATH");
            String javaHome = System.getProperty("java.home");
            String binPath = javaHome + File.separator + "bin";
            path = binPath + ";" + path;
            pb.environment().put("PATH", path);
        }
    }

    public static int exec(String... args) throws IOException {
        if (getVerbosity() >= 2) {
            logger.info("CMD: " + String.join(" ", args));
        }
        ProcessBuilder pb = new ProcessBuilder(args);
        addJavaBinPath(pb);
        Process p = pb.redirectErrorStream(true).start();
        int ret = 127;
        byte[] buf = new byte[16 * 1024];
        try {
            InputStream is = p.getInputStream();
            int n = is.read(buf);
            while (n > 0) {
                n = is.read(buf);
            }
            ret = p.waitFor();
        } catch (InterruptedException e) {
            logger.severe("Unexpected interruption", e);
        }

        return ret;
    }

    public static class Result {
        public Result(int ret, byte[] stdOutErr) {
            this.ret = ret;
            this.stdOutErr = stdOutErr;
        }
        public int ret;
        public byte[] stdOutErr;
    }

    /**
     * Exec command
     * @param args arguments
     * @return instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResult(String... args) throws IOException {
        if (getVerbosity() >= 2) {
            logger.info("CMD: " + String.join(" ", args));
        }
        ProcessBuilder pb = new ProcessBuilder(args);
        addJavaBinPath(pb);
        Process p = pb.redirectErrorStream(true).start();
        int ret = 127;
        byte[] buf = new byte[16 * 1024];
        ByteArrayOutputStream out = new ByteArrayOutputStream(10 * 1024);
        try {
            InputStream is = p.getInputStream();
            int n = is.read(buf);
            while (n > 0) {
                out.write(buf, 0, n);
                n = is.read(buf);
            }
            ret = p.waitFor();
        } catch (InterruptedException e) {
            logger.severe("Unexpected interruption", e);
        }

        return new Result(ret, out.toByteArray());
    }

    private static ProcessBuilder processBuilderWithArgs(Map<String, String> env, String[] args) {
        if (getVerbosity() >= 2) {
            logger.info("CMD: " + String.join(" ", args));
        }
        ProcessBuilder pb = new ProcessBuilder(args);
        addJavaBinPath(pb);
        pb.redirectErrorStream(true);

        Map<String, String> pbenv = pb.environment();
        for (Map.Entry<String, String> entry : env.entrySet())
        {
            pbenv.put(entry.getKey(), entry.getValue());
        }

        return pb;
    }

    private static Result runProcessBuilder(ProcessBuilder pb) throws IOException {
        Process p = pb.start();
        int ret = 127;
        byte[] buf = new byte[16 * 1024];
        ByteArrayOutputStream out = new ByteArrayOutputStream(10 * 1024);
        try {
            InputStream is = p.getInputStream();
            int n = is.read(buf);
            while (n > 0) {
                out.write(buf, 0, n);
                n = is.read(buf);
            }
            ret = p.waitFor();
        } catch (InterruptedException e) {
            logger.severe("Unexpected interruption", e);
        }

        return new Result(ret, out.toByteArray());
    }

    public static Result execResultWithEnvironment(Map<String, String> env, String... args) throws IOException {
        ProcessBuilder pb = processBuilderWithArgs(env, args);
        return runProcessBuilder(pb);
    }

    public static Result execResultWithEnvironmentWorkDir(Map<String, String> env, File workDir, String... args) throws IOException {
        ProcessBuilder pb = processBuilderWithArgs(env, args);
        pb.directory(workDir);
        return runProcessBuilder(pb);
    }

    public static Result execResultWithEnvironment(Map<String, String> env, List<String> args) throws IOException {
        String[] array = new String[args.size()];
        array = args.toArray(array);

        return Exec.execResultWithEnvironment(env, array);
    }

}
