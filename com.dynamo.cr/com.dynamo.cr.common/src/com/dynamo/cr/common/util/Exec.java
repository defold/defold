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
