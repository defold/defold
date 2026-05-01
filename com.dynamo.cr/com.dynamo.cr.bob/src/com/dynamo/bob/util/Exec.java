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

package com.dynamo.bob.util;
import com.dynamo.bob.Platform;

import java.nio.file.Files;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.logging.Logger;

public class Exec {

    private static String verbosity = System.getenv("DM_BOB_VERBOSE");
    private static Logger logger = Logger.getLogger(Exec.class.getName());


    public static class Result {
        public Result(int ret, byte[] stdOutErr) {
            this.ret = ret;
            this.stdOutErr = stdOutErr;
        }
        public int ret;
        public byte[] stdOutErr;
    }

    private static int getVerbosity() {
        if (verbosity == null)
            return 0;
        try {
            return Integer.parseInt(verbosity);
        } catch (NumberFormatException nfe) {
            return 0;
        }
    }

    private static int startAndWaitForProcessBuilder(ProcessBuilder pb, OutputStream os) throws IOException {
        Process p = pb.start();

        int ret = 127;
        if (os != null) {
            InputStream is = p.getInputStream();
            while (p.isAlive() || is.available() > 0) {
                byte[] bytes = is.readNBytes(1024);
                if (bytes.length > 0) {
                    os.write(bytes);
                }
            }
            ret = p.exitValue();
            is.close();
            os.flush();
        }
        else {
            try {
                ret = p.waitFor();
            }
            catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        return ret;
    }
    private static Result startAndWaitForProcessBuilderResult(ProcessBuilder pb) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream(10 * 1024);
        int ret = startAndWaitForProcessBuilder(pb, baos);
        return new Result(ret, baos.toByteArray());
    }

    private static ProcessBuilder createProcessBuilder(String... args) throws IOException {
        if (getVerbosity() >= 2) {
            logger.info("CMD: " + String.join(" ", args));
        }

        ProcessBuilder processBuilder = new ProcessBuilder(args);
        processBuilder.redirectErrorStream(true);

        Platform platform = Platform.getHostPlatform();
        if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32) {
            // On Windows `exe` files often require vcruntime140_1.dll and vcruntime140.dll
            // these files are available in jdk/bin folder
            // see https://github.com/defold/defold/issues/8277#issuecomment-1836823183
            String path = System.getenv("PATH");
            String javaHome = System.getProperty("java.home");
            String binPath = javaHome + File.separator + "bin";
            path = binPath + ";" + path;
            processBuilder.environment().put("PATH", path);
        }
        return processBuilder;
    }

    /**
     * Exec command
     * @param args arguments
     * @return Exit code of the command
     * @throws IOException
     */
    public static int exec(String... args) throws IOException {
        ProcessBuilder pb = createProcessBuilder(args);
        return startAndWaitForProcessBuilder(pb, null);
    }

    /**
     * Exec command
     * @param args arguments
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResult(String... args) throws IOException {
        ProcessBuilder pb = createProcessBuilder(args);
        return startAndWaitForProcessBuilderResult(pb);
    }

    /**
     * Exec command
     * @param env environment variables to use
     * @param args arguments
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResultWithEnvironment(Map<String, String> env, String... args) throws IOException {
        ProcessBuilder pb = createProcessBuilder(args);
        pb.environment().putAll(env);
        return startAndWaitForProcessBuilderResult(pb);
    }

    /**
     * Exec command
     * @param env environment variables to use
     * @param workDir working directory to run command from
     * @param args arguments
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResultWithEnvironmentWorkDir(Map<String, String> env, File workDir, String... args) throws IOException {
        ProcessBuilder pb = createProcessBuilder(args);
        pb.environment().putAll(env);
        pb.directory(workDir);
        return startAndWaitForProcessBuilderResult(pb);
    }

    /**
     * Exec command
     * @param env environment variables to use
     * @param args arguments
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResultWithEnvironment(Map<String, String> env, List<String> args) throws IOException {
        String[] array = new String[args.size()];
        array = args.toArray(array);
        ProcessBuilder pb = createProcessBuilder(array);
        pb.environment().putAll(env);
        return startAndWaitForProcessBuilderResult(pb);
    }

}
