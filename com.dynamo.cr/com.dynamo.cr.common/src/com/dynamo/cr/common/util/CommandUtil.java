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

package com.dynamo.cr.common.util;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;

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

    public static Result execCommand(String working_dir, Map<String, String> env, IListener listener, String[] command) throws IOException {
        ProcessBuilder pb = new ProcessBuilder(command);
        if (working_dir != null)
            pb.directory(new File(working_dir));

        if (env != null)
        	pb.environment().putAll(env);

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
                    e1.printStackTrace();
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

        return res;
    }

    public static Result execCommand(String[] command) throws IOException {
        return execCommand(null, null, null, command);
    }

}
