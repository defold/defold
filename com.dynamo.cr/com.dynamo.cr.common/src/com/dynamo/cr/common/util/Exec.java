package com.dynamo.cr.common.util;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.logging.*;

public class Exec {

    private static Logger logger = Logger.getLogger(Exec.class.getCanonicalName());

    /**
     * Exec command
     * @note use only this function for "simple" stuff as stdout/stderr isn't returned. See execResult()
     * @param args arguments
     * @return return code
     * @throws IOException
     */
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


}
