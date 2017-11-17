package com.dynamo.bob.util;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.List;
import java.util.Map;

public class Exec {

    private static Logger logger = Logger.getLogger(Exec.class.getCanonicalName());

    public static int exec(String... args) throws IOException {
        Process p = new ProcessBuilder(args).redirectErrorStream(true).start();
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
            logger.log(Level.SEVERE, "Unexpected interruption", e);
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
        Process p = new ProcessBuilder(args).redirectErrorStream(true).start();
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
            logger.log(Level.SEVERE, "Unexpected interruption", e);
        }

        return new Result(ret, out.toByteArray());
    }

    private static ProcessBuilder processBuilderWithArgs(Map<String, String> env, String[] args) {
        ProcessBuilder pb = new ProcessBuilder(args);
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
            logger.log(Level.SEVERE, "Unexpected interruption", e);
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
