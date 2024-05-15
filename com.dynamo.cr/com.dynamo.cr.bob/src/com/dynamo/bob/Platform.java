// Copyright 2020-2024 The Defold Foundation
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

package com.dynamo.bob;

import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;

import com.dynamo.bob.util.StringUtil;

import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;


public enum Platform {
    // arch, is64bit, os, exeSuffixes, exePrefix, libSuffix, libPrefix, extenderPaths, architectures, extenderPair
    //    extenderPaths: The extenderPaths are the searh directories that we use when looking for platform resources for a remote build
    X86_64MacOS( "x86_64",  true,   "macos",   new String[] {""}, "", "lib", ".dylib", new String[] {"osx", "x86_64-osx"}, PlatformArchitectures.MacOS, "x86_64-osx"),
    Arm64MacOS(  "arm64",   true,   "macos",   new String[] {""}, "", "lib", ".dylib", new String[] {"osx", "arm64-osx"}, PlatformArchitectures.MacOS, "arm64-osx"),
    X86Win32(    "x86",     false,  "win32",   new String[] {".exe"}, "", "", ".dll", new String[] {"win32", "x86-win32"}, PlatformArchitectures.Windows32, "x86-win32"),
    X86_64Win32( "x86_64",  true,   "win32",   new String[] {".exe"}, "", "", ".dll", new String[] {"win32", "x86_64-win32"}, PlatformArchitectures.Windows64, "x86_64-win32"),
    X86Linux(    "x86",     false,  "linux",   new String[] {""}, "", "lib", ".so", new String[] {"linux", "x86-linux"}, PlatformArchitectures.Linux, "x86-linux"),
    X86_64Linux( "x86_64",  true,   "linux",   new String[] {""}, "", "lib", ".so", new String[] {"linux", "x86_64-linux"}, PlatformArchitectures.Linux, "x86_64-linux"),
    Arm64Ios(    "arm64",   true,   "ios",     new String[] {""}, "", "lib", ".so", new String[] {"ios", "arm64-ios"}, PlatformArchitectures.iOS, "arm64-ios"),
    X86_64Ios(   "x86_64",  true,   "ios",     new String[] {""}, "", "lib", ".so", new String[] {"ios", "x86_64-ios"}, PlatformArchitectures.iOS, "x86_64-ios"),
    Armv7Android("armv7",   false,  "android", new String[] {".so"}, "lib", "lib", ".so", new String[] {"android", "armv7-android"}, PlatformArchitectures.Android, "armv7-android"),
    Arm64Android("arm64",   true,   "android", new String[] {".so"}, "lib", "lib", ".so", new String[] {"android", "arm64-android"}, PlatformArchitectures.Android, "arm64-android"),
    JsWeb(       "js",      true,   "web",     new String[] {".js"}, "", "lib", "", new String[] {"web", "js-web"}, PlatformArchitectures.Web, "js-web"),
    WasmWeb(     "wasm",    true,   "web",     new String[] {".js", ".wasm"}, "", "lib", "", new String[] {"web", "wasm-web"}, PlatformArchitectures.Web, "wasm-web"),
    Arm64NX64(   "arm64",   true,   "nx64",    new String[] {".nss"}, "", "", "", new String[] {"nx64", "arm64-nx64"}, PlatformArchitectures.NX64, "arm64-nx64"),
    X86_64PS4(   "x86_64",  true,   "ps4",     new String[] {".elf"}, "", "", "", new String[] {"ps4", "x86_64-ps4"}, PlatformArchitectures.PS4, "x86_64-ps4"),
    X86_64PS5(   "x86_64",  true,   "ps5",     new String[] {".elf"}, "", "", "", new String[] {"ps5", "x86_64-ps5"}, PlatformArchitectures.PS5, "x86_64-ps5");

    private static HashMap<OS, String> platformPatterns = new HashMap<OS, String>();
    static {
        // See graphics_ddf.proto
        platformPatterns.put(PlatformProfile.OS.OS_ID_GENERIC, "^$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_WINDOWS, "^x86(_64)?-win32$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_OSX,     "^x86(_64)?-macos$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_LINUX,   "^x86(_64)?-linux$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_IOS,     "^((arm64)|(x86_64))-ios$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_ANDROID, "^arm((v7)|(64))-android$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_WEB,     "^((js)|(wasm))-web$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_SWITCH,  "^(arm64-nx64)$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_PS4,     "^(x86_64-ps4)$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_PS5,     "^(x86_64-ps5)$");
    }


    public static boolean matchPlatformAgainstOS(String platform, PlatformProfile.OS os) {
        if (os == PlatformProfile.OS.OS_ID_GENERIC) {
            return true;
        }

        String platformPattern = platformPatterns.get(os);
        if (platformPattern != null && platform.matches(platformPattern)) {
            return true;
        }

        return false;
    }

    String arch, os;
    Boolean isPlatform64bit;
    String[] exeSuffixes;
    String exePrefix;
    String libSuffix;
    String libPrefix;
    String[] extenderPaths = null;
    PlatformArchitectures architectures;
    String extenderPair;
    Platform(String arch, boolean isPlatform64bit, String os, String[] exeSuffixes, String exePrefix, String libPrefix, String libSuffix, String[] extenderPaths, PlatformArchitectures architectures, String extenderPair) {
        this.arch = arch;
        this.isPlatform64bit = isPlatform64bit;
        this.os = os;
        this.exeSuffixes = exeSuffixes;
        this.exePrefix = exePrefix;
        this.libSuffix = libSuffix;
        this.libPrefix = libPrefix;
        this.extenderPaths = extenderPaths;
        this.architectures = architectures;
        this.extenderPair = extenderPair;
    }

    public Boolean is64bit() {
        return isPlatform64bit;
    }

    public String[] getExeSuffixes() {
        return exeSuffixes;
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

    public String[] getExtenderPaths() {
        return extenderPaths;
    }

    public String getPair() {
        return String.format("%s-%s", this.arch, this.os);
    }

    public String getOs() {
        return this.os;
    }

    public String getExtenderPair() {
        return extenderPair;
    }

    public PlatformArchitectures getArchitectures() {
        return architectures;
    }

    public static List<Platform> getArchitecturesFromString(String architectures, Platform defaultPlatform) {

        String[] architecturesStrings;
        if (architectures == null || architectures.length() == 0) {
            architecturesStrings = defaultPlatform.getArchitectures().getDefaultArchitectures();
        }
        else {
            architecturesStrings = architectures.split(",");
        }

        List<Platform> out = new ArrayList<Platform>();
        for (String architecture : architecturesStrings) {
            out.add(Platform.get(architecture));
        }
        return out;
    }

    public String toString() {
        return getExtenderPair();
    }

    public List<String> formatBinaryName(String basename) {
        List<String> names = new ArrayList<String>();
        for (String exeSuffix : exeSuffixes) {
            names.add(exePrefix + basename + exeSuffix);
        }
        return names;
    }

    public String formatLibraryName(String basename) {
        return libPrefix + basename + libSuffix;
    }

    public static Platform get(String pair) {
        // support for legacy platform name (until we've changed all occurrances to "x86-win32")
        if (pair.equals("win32"))
            pair = "x86_64-win32";

        Platform[] platforms = Platform.values();
        for (Platform p : platforms) {
            if (p.getPair().equals(pair)) {
                return p;
            }
        }
        return null;
    }

    public static Platform getJavaPlatform() {
        String os_name = StringUtil.toLowerCase(System.getProperty("os.name"));
        String arch = StringUtil.toLowerCase(System.getProperty("os.arch"));

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Win32;
            }
            else {
                return Platform.X86Win32;
            }
        } else if (os_name.indexOf("mac") != -1) {
            return Platform.X86_64MacOS;
        } else if (os_name.indexOf("linux") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Linux;
            } else {
                return Platform.X86Linux;
            }
        } else {
            throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
        }
    }

    public static Platform getHostPlatform() {
        String os_name = StringUtil.toLowerCase(System.getProperty("os.name"));
        String arch = StringUtil.toLowerCase(System.getProperty("os.arch"));

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Win32;
            }
            else {
                return Platform.X86Win32;
            }
        } else if (os_name.indexOf("mac") != -1) {
            // Intel java reports: os_name: mac os x  arch: x86_64
            // Arm java reports: os_name: mac os x  arch: aarch64
            if (arch.equals("x86_64"))
                return Platform.X86_64MacOS;
            else if (arch.equals("aarch64"))
                return Platform.Arm64MacOS;
        } else if (os_name.indexOf("linux") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Linux;
            } else {
                return Platform.X86Linux;
            }
        }

        throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
    }
}
