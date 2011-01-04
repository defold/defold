package com.dynamo.server.git;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.logging.Level;
import java.util.logging.Logger;

public class CommandUtil {

    public static class Result {
        public StringBuffer stdOut = new StringBuffer();
        public StringBuffer stdErr = new StringBuffer();
        public int exitValue = 0;
    }

    public interface IListener {
        public void onStdErr(StringBuffer buffer);
        public void onStdOut(StringBuffer buffer);
    }

    public static Result execCommand(String working_dir, IListener listener, String[] command) throws IOException {
        ProcessBuilder pb = new ProcessBuilder(command);
        if (working_dir != null)
            pb.directory(new File(working_dir));

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
                    Logger.getLogger(Logger.GLOBAL_LOGGER_NAME).log(Level.SEVERE, e.getMessage(), e);
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
            Logger.getLogger(Logger.GLOBAL_LOGGER_NAME).log(Level.SEVERE, e.getMessage(), e);
        }

        return res;
    }

    public static Result execCommand(String[] command) throws IOException {
        return execCommand(null, null, command);
    }

}
