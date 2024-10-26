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

import java.nio.file.Files;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.InputStream;
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

    /**
     * Wraps a process builder and redirects stdout (and stderr) to a file.
     */
    private static class RedirectedProcessBuilder { 
        private static Logger logger = Logger.getLogger(RedirectedProcessBuilder.class.getName());

        private File stdoutFile;
        private ProcessBuilder processBuilder;

        public RedirectedProcessBuilder(String... command) throws IOException, FileNotFoundException {
            processBuilder = new ProcessBuilder(command);
            stdoutFile = File.createTempFile("exec", null);
            FileOutputStream outputStream = new FileOutputStream(stdoutFile);
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
        }

        public Map<String,String> environment() {
            return processBuilder.environment();
        }

        public RedirectedProcessBuilder directory(File directory) {
            processBuilder.directory(directory);
            return this;
        }

        public File getStdoutFile() {
            return stdoutFile;
        }

        public InputStream getInputStream() throws FileNotFoundException {
            return new FileInputStream(stdoutFile);
        }

        public Process start() throws IOException {
            return processBuilder.start();
        }
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

    private static int startAndWaitForProcessBuilder(RedirectedProcessBuilder pb) throws IOException {
        Process p = pb.start();
        int ret = 127;
        try {
            InputStream is = pb.getInputStream();
            while (p.isAlive()) {
                byte[] bytes = is.readNBytes(1024);
                if (bytes.length > 0) {
                    logger.info(new String(bytes));
                }
            }
            ret = p.waitFor();
            is.close();
        } catch (InterruptedException e) {
            logger.severe("Unexpected interruption", e);
        }
        return ret;
    }
    private static Result startAndWaitForProcessBuilderResult(RedirectedProcessBuilder pb) throws IOException {
        int ret = startAndWaitForProcessBuilder(pb);
        byte[] out = Files.readAllBytes(pb.getStdoutFile().toPath());
        return new Result(ret, out);
    }

    private static RedirectedProcessBuilder createProcessBuilder(String[] args) throws IOException {
        if (getVerbosity() >= 2) {
            logger.info("CMD: " + String.join(" ", args));
        }
        RedirectedProcessBuilder pb = new RedirectedProcessBuilder(args);
        return pb;
    }

    /**
     * Exec command
     * @param args arguments
     * @return Exit code of the command
     * @throws IOException
     */
    public static int exec(String... args) throws IOException {
        RedirectedProcessBuilder pb = createProcessBuilder(args);
        return startAndWaitForProcessBuilder(pb);
    }

    /**
     * Exec command
     * @param args arguments
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResult(String... args) throws IOException {
        RedirectedProcessBuilder pb = createProcessBuilder(args);
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
        RedirectedProcessBuilder pb = createProcessBuilder(args);
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
        RedirectedProcessBuilder pb = createProcessBuilder(args);
        pb.environment().putAll(env);
        pb.directory(workDir);
        return startAndWaitForProcessBuilderResult(pb);
    }

    /**
     * Exec command
     * @param env environment variables to use
     * @return Result instance with return code and stdout/stderr combined
     * @throws IOException
     */
    public static Result execResultWithEnvironment(Map<String, String> env, List<String> args) throws IOException {
        String[] array = new String[args.size()];
        array = args.toArray(array);
        RedirectedProcessBuilder pb = createProcessBuilder(array);
        pb.environment().putAll(env);
        return startAndWaitForProcessBuilderResult(pb);
    }

}
