// Copyright 2020-2025 The Defold Foundation
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

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;

import java.lang.reflect.Field;

import com.dynamo.bob.util.StringUtil;

import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;


public class Platform {
    // os, arch, is64bit, osName, exeSuffixes, exePrefix, libSuffix, libPrefix, extenderPaths, architectures, extenderPair
    //    extenderPaths: The extenderPaths are the search directories that we use when looking for platform resources for a remote build
    public static final Platform X86_64MacOS    = new Platform(OS.OS_ID_OSX,        "x86_64",       true,    "macos",   new String[] {""},               "",     "lib",  ".dylib",   new String[] {"osx", "x86_64-osx"},         PlatformArchitectures.MacOS,        "x86_64-osx");
    public static final Platform Arm64MacOS     = new Platform(OS.OS_ID_OSX,        "arm64",        true,    "macos",   new String[] {""},               "",     "lib",  ".dylib",   new String[] {"osx", "arm64-osx"},          PlatformArchitectures.MacOS,        "arm64-osx");
    public static final Platform X86Win32       = new Platform(OS.OS_ID_WINDOWS,    "x86",          false,   "win32",   new String[] {".exe"},           "",     "",     ".dll",     new String[] {"win32", "x86-win32"},        PlatformArchitectures.Windows32,    "x86-win32");
    public static final Platform X86_64Win32    = new Platform(OS.OS_ID_WINDOWS,    "x86_64",       true,    "win32",   new String[] {".exe"},           "",     "",     ".dll",     new String[] {"win32", "x86_64-win32"},     PlatformArchitectures.Windows64,    "x86_64-win32");
    public static final Platform X86_64Linux    = new Platform(OS.OS_ID_LINUX,      "x86_64",       true,    "linux",   new String[] {""},               "",     "lib",  ".so",      new String[] {"linux", "x86_64-linux"},     PlatformArchitectures.Linux,        "x86_64-linux");
    public static final Platform Arm64Linux     = new Platform(OS.OS_ID_LINUX,      "arm64",        true,    "linux",   new String[] {""},               "",     "lib",  ".so",      new String[] {"linux", "arm64-linux"},      PlatformArchitectures.LinuxArm64,   "arm64-linux");
    public static final Platform Arm64Ios       = new Platform(OS.OS_ID_IOS,        "arm64",        true,    "ios",     new String[] {""},               "",     "lib",  ".so",      new String[] {"ios", "arm64-ios"},          PlatformArchitectures.iOS,          "arm64-ios");
    public static final Platform X86_64Ios      = new Platform(OS.OS_ID_IOS,        "x86_64",       true,    "ios",     new String[] {""},               "",     "lib",  ".so",      new String[] {"ios", "x86_64-ios"},         PlatformArchitectures.iOS,          "x86_64-ios");
    public static final Platform Armv7Android   = new Platform(OS.OS_ID_ANDROID,    "armv7",        false,   "android", new String[] {".so"},            "lib", "lib",   ".so",      new String[] {"android", "armv7-android"},  PlatformArchitectures.Android,      "armv7-android");
    public static final Platform Arm64Android   = new Platform(OS.OS_ID_ANDROID,    "arm64",        true,    "android", new String[] {".so"},            "lib", "lib",   ".so",      new String[] {"android", "arm64-android"},  PlatformArchitectures.Android,      "arm64-android");
    public static final Platform JsWeb          = new Platform(OS.OS_ID_WEB,        "js",           true,    "web",     new String[] {".js"},            "",     "lib",  "",         new String[] {"web", "js-web"},             PlatformArchitectures.Web,          "js-web");
    public static final Platform WasmWeb        = new Platform(OS.OS_ID_WEB,        "wasm",         true,    "web",     new String[] {".js", ".wasm"},   "",     "lib",  "",         new String[] {"web", "wasm-web"},           PlatformArchitectures.Web,          "wasm-web");
    public static final Platform WasmPthreadWeb = new Platform(OS.OS_ID_WEB,        "wasm_pthread", true,    "web",     new String[] {".js", ".wasm"},   "",     "lib",  "",         new String[] {"web", "wasm_pthread-web"},   PlatformArchitectures.Web,          "wasm_pthread-web");

    // TODO: add these from the extension plugins instead!
    public static final Platform Arm64NX64      = new Platform(OS.OS_ID_SWITCH,     "arm64",        true,   "nx64",     new String[] {".nss"},           "",     "",     "",         new String[] {"nx64", "arm64-nx64"},        PlatformArchitectures.NX64,         "arm64-nx64");
    public static final Platform X86_64PS4      = new Platform(OS.OS_ID_PS4,        "x86_64",       true,   "ps4",      new String[] {".elf"},           "",     "",     "",         new String[] {"ps4", "x86_64-ps4"},         PlatformArchitectures.PS4,          "x86_64-ps4");
    public static final Platform X86_64PS5      = new Platform(OS.OS_ID_PS5,        "x86_64",       true,   "ps5",      new String[] {".elf"},           "",     "",     "",         new String[] {"ps5", "x86_64-ps5"},         PlatformArchitectures.PS5,          "x86_64-ps5");

    private static final HashMap<String, Platform> map = new HashMap<>();

    static {
        loadClassData();
    }

    private static void loadClassData() {
        Arrays.stream(Platform.class.getDeclaredFields())
                .filter(declaredField -> declaredField.getType() == Platform.class)
                .forEach(Platform::putInMap);
    }

    private static void putInMap(Field declaredField) {
        try {
            map.putIfAbsent(declaredField.getName(), (Platform) declaredField.get(null));
        } catch (IllegalAccessException e) {
            System.err.println("Could not initialize Platform map value: " + declaredField.getName() + " " + e);
        }
    }

    public static Platform[] values() {
        return map.values().toArray(Platform[]::new).clone();
    }

    public static Platform valueOf(String name) {
        Platform platform = map.get(name);
        if (platform == null) {
            throw new IllegalArgumentException("No Platform by the name " + name + " found");
        }
        return platform;
    }

    @Override
    public boolean equals(Object o) {
        return this == o;
    }

    OS     osID;
    String arch;
    String osName;
    Boolean isPlatform64bit;
    String[] exeSuffixes;
    String exePrefix;
    String libSuffix;
    String libPrefix;
    String[] extenderPaths = null;
    PlatformArchitectures architectures;
    String extenderPair;

    Platform(OS os, String arch, boolean isPlatform64bit, String osName,
             String[] exeSuffixes, String exePrefix, String libPrefix, String libSuffix,
             String[] extenderPaths, PlatformArchitectures architectures, String extenderPair) {
        this.osID = os;
        this.arch = arch;
        this.isPlatform64bit = isPlatform64bit;
        this.osName = osName;
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
        return String.format("%s-%s", this.arch, this.osName);
    }

    public OS getOsID() {
        return this.osID;
    }

    public String getOs() {
        return this.osName;
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

    public boolean isWindows() {
        return this.osID.equals(PlatformProfile.OS.OS_ID_WINDOWS);
    }

    public boolean isLinux() {
        return this.osID.equals(PlatformProfile.OS.OS_ID_LINUX);
    }

    public boolean isMacOS() {
        return this.osID.equals(PlatformProfile.OS.OS_ID_OSX);
    }

    public boolean matchesPair(String pair) {
        Platform platform = Platform.get(pair);
        return this == platform;
    }

    public boolean matchesOS(PlatformProfile.OS os) {
        if (os.equals(OS.OS_ID_GENERIC)) {
            return true;
        }
        return this.osID.equals(os);
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
            } else if (arch.equals("aarch64")) {
                return Platform.Arm64Linux;
            }
        }

        throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
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
            }
            else if (arch.equals("aarch64")) {
                return Platform.Arm64Linux;
            }
        }

        throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
    }
}
