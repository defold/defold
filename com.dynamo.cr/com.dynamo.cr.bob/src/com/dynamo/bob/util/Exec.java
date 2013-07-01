package com.dynamo.bob.util;

import java.io.IOException;
import java.io.InputStream;
import java.util.logging.Level;
import java.util.logging.Logger;

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

}
