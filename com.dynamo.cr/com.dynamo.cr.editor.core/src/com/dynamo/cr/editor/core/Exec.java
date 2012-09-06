package com.dynamo.cr.editor.core;

import java.io.IOException;
import java.io.InputStream;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Exec {

    private static Logger logger = LoggerFactory.getLogger(Exec.class);

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
            logger.error("Unexpected interruption", e);
        }

        return ret;
    }

}
