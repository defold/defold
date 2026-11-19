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

package com.defold.editor.test;

import com.sun.jna.Function;
import com.sun.jna.NativeLibrary;
import java.io.File;

/** Small child-process behaviors that do not require a Clojure runtime. */
public class ProcessTestHelper {
    public static void main(String[] args) throws Exception {
        switch (args[0]) {
            case "stdout":
                System.out.print(args[1]);
                break;
            case "stderr":
                System.err.print(args[1]);
                break;
            case "streams":
                System.out.print("stdout");
                System.out.flush();
                System.err.print("stderr");
                break;
            case "env":
                String value = System.getenv(args[1]);
                System.out.print(value == null ? "<unset>" : value);
                break;
            case "require-env":
                System.exit(System.getenv(args[1]) == null ? 1 : 0);
                break;
            case "directory":
                System.out.print(new File(".").getCanonicalPath());
                break;
            case "copy-stdin":
                System.in.transferTo(System.out);
                break;
            case "ready":
                System.out.println("ready");
                System.out.flush();
                System.in.read();
                break;
            case "arguments":
                for (int i = 1; i < args.length; i++) {
                    System.out.print("arg:");
                    for (char character : args[i].toCharArray()) {
                        System.out.printf("%04x", (int) character);
                    }
                    System.out.println();
                }
                break;
            case "command-line":
                System.out.print(NativeLibrary.getInstance("kernel32")
                        .getFunction("GetCommandLineW", Function.ALT_CONVENTION)
                        .invokePointer(new Object[0]).getWideString(0));
                break;
            case "exit":
                System.exit(Integer.parseInt(args[1]));
                break;
            case "error-mode":
                System.out.print(NativeLibrary.getInstance("kernel32")
                        .getFunction("GetErrorMode", Function.ALT_CONVENTION)
                        .invokeInt(new Object[0]));
                break;
            case "noop":
                break;
            default:
                throw new IllegalArgumentException("Unknown process test command: " + args[0]);
        }
        System.out.flush();
        System.err.flush();
    }
}
