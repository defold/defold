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

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeFalse;

import java.io.File;
import java.nio.charset.StandardCharsets;

import org.junit.Test;

import com.dynamo.bob.Platform;

public class ExecTest {

    public static class PrintChildEnvironment {
        public static void main(String[] args) throws Exception {
            Exec.Result result = Exec.execResult("/bin/sh", "-c", "printf '%s' \"${CFProcessPath-unset}\"");
            System.out.print(System.getenv("CFProcessPath") + "|" + new String(result.stdOutErr, StandardCharsets.UTF_8));
        }
    }

    // Verifies tools started through Exec don't inherit CFProcessPath, which made each tool appear as the macOS editor with its own Dock icon.
    @Test
    public void testChildProcessDoesNotInheritCFProcessPath() throws Exception {
        assumeFalse(Platform.getHostPlatform().isWindows());

        String java = System.getProperty("java.home") + File.separator + "bin" + File.separator + "java";
        ProcessBuilder pb = new ProcessBuilder(java, "-cp", System.getProperty("java.class.path"), PrintChildEnvironment.class.getName());
        pb.environment().put("CFProcessPath", "/Applications/Defold.app/Contents/MacOS/Defold");
        pb.redirectErrorStream(true);
        Process process = pb.start();
        String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);

        assertEquals(output, 0, process.waitFor());
        assertEquals("/Applications/Defold.app/Contents/MacOS/Defold|unset", output);
    }
}
