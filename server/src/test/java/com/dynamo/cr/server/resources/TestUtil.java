package com.dynamo.cr.server.resources;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;

class TestUtil {

    private static Logger logger = LoggerFactory.getLogger(TestUtil.class);

    static class Result {
        StringBuffer stdOut = new StringBuffer();
        StringBuffer stdErr = new StringBuffer();
        int exitValue = 0;
    }

    interface IListener {
        void onStdErr(StringBuffer buffer);
        void onStdOut(StringBuffer buffer);
    }

    private static Result execCommand(String workingDir, IListener listener, String[] command) throws IOException {
        return execCommand(workingDir, listener, command, null);
    }

    private static Result execCommand(String workingDir, IListener listener, String[] command, Map<String, String> env) throws IOException {
        ProcessBuilder pb = new ProcessBuilder(command);
        if (workingDir != null)
            pb.directory(new File(workingDir));

        if (env != null) {
            Map<String, String> environment = pb.environment();
            environment.putAll(env);
        }

        Process p = pb.start();
        InputStream std = p.getInputStream();
        InputStream err = p.getErrorStream();

        Result res = new Result();

        boolean done = false;
        while (!done) {
            try {
                res.exitValue = p.exitValue();
                done = true;
            } catch (IllegalThreadStateException e) {
                try {
                    Thread.sleep(2);
                } catch (InterruptedException e1) {
                    logger.error(e.getMessage(), e);
                }
            }

            // Do whatever you want with the output
            while (std.available() > 0) {
                byte[] tmp = new byte[std.available()];
                std.read(tmp);
                res.stdOut.append(new String(tmp));

                if (listener != null)
                    listener.onStdOut(res.stdOut);
            }

            while (err.available() > 0) {
                byte[] tmp = new byte[err.available()];
                err.read(tmp);
                res.stdErr.append(new String(tmp));

                if (listener != null)
                    listener.onStdErr(res.stdErr);
            }
        }

        std.close();
        err.close();
        try {
            p.waitFor();
        } catch (InterruptedException e) {
            logger.error(e.getMessage(), e);
        }

        return res;
    }

    static Result execCommand(String[] command) throws IOException {
        return execCommand(null, null, command);
    }
}
