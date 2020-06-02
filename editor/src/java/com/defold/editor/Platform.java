// Copyright 2020 The Defold Foundation
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

package com.defold.editor;


public enum Platform {
    X86_64Darwin("x86_64", "darwin", "", "", "lib", ".dylib"),
    X86Win32("x86", "win32", ".exe", "", "", ".dll"),
    X86_64Win32("x86_64", "win32", ".exe", "", "", ".dll"),
    X86_64Linux("x86_64", "linux", "", "", "lib", ".so"),
    Armv7Darwin("armv7", "darwin", "", "", "lib", ".so"),
    Arm64Darwin("arm64", "darwin", "", "", "lib", ".so"),
    Armv7Android("armv7", "android", ".so", "lib", "lib", ".so"),
    Arm64Android("arm64", "android", ".so", "lib", "lib", ".so"),
    JsWeb("js", "web", ".js", "", "lib", "");

    String arch, os;
    String exeSuffix;
    String exePrefix;
    String libSuffix;
    String libPrefix;
    Platform(String arch, String os, String exeSuffix, String exePrefix, String libPrefix, String libSuffix) {
        this.arch = arch;
        this.os = os;
        this.exeSuffix = exeSuffix;
        this.exePrefix = exePrefix;
        this.libSuffix = libSuffix;
        this.libPrefix = libPrefix;
    }

    public String getOs() {
        return os;
    }

    public String getExeSuffix() {
        return exeSuffix;
    }

    public String getExePrefix() {
        return exePrefix;
    }

    public String getLibPrefix() {
        return libPrefix;
    }

    public String getLibSuffix() {
        return libSuffix;
    }

    public String getPair() {
        return String.format("%s-%s", this.arch, this.os);
    }

    static Platform get(String pair) {
        Platform[] platforms = Platform.values();
        for (Platform p : platforms) {
            if (p.getPair().equals(pair)) {
                return p;
            }
        }
        return null;
    }

    public static Platform createPlatform(String os, String arch)
    {
        if (os.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Win32;
            } else {
                return Platform.X86Win32;
            }
        } else if (os.indexOf("mac") != -1) {
            return Platform.X86_64Darwin;
        } else if (os.indexOf("linux") != -1) {
            return Platform.X86_64Linux;
        } else {
            throw new RuntimeException(String.format("Could not identify OS + arch: '%s %s'", os, arch));
        }
    }

    public static Platform getHostPlatform()
    {
        String os_name = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();
        return createPlatform(os_name, arch);
    }

    public static Platform getJavaPlatform()
    {
        // Useful for determining f.i. which native dynamic library to load: must be same bitness as jvm

        String os_name = System.getProperty("os.name").toLowerCase();
        String bitness = System.getProperty("sun.arch.data.model");

        String arch = "unknown";

        if (bitness.equals("32")) {
            arch = "x86";
        }
        else if (bitness.equals("64")) {
            arch = "x86_64";
        }

        return createPlatform(os_name, arch);
    }
}
