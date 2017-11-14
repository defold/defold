package com.dynamo.bob;

import java.util.HashMap;

import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;


public enum Platform {
    X86Darwin("x86", "darwin", "", "", "lib", ".dylib", new String[] {"osx", "x86-osx"}, PlatformArchitectures.OSX, "x86-osx"),
    X86_64Darwin("x86_64", "darwin", "", "", "lib", ".dylib", new String[] {"osx", "x86_64-osx"}, PlatformArchitectures.OSX, "x86_64-osx"),
    X86Win32("x86", "win32", ".exe", "", "", ".dll", new String[] {"win32", "x86-win32"}, PlatformArchitectures.Windows, "x86-win32"),
    X86_64Win32("x86_64", "win32", ".exe", "", "", ".dll", new String[] {"win32", "x86_64-win32"}, PlatformArchitectures.Windows, "x86_64-win32"),
    X86Linux("x86", "linux", "", "", "lib", ".so", new String[] {"linux", "x86-linux"}, PlatformArchitectures.Linux, "x86-linux"),
    X86_64Linux("x86_64", "linux", "", "", "lib", ".so", new String[] {"linux", "x86_64-linux"}, PlatformArchitectures.Linux, "x86_64-linux"),
    Armv7Darwin("armv7", "darwin", "", "", "lib", ".so", new String[] {"ios", "armv7-ios"}, PlatformArchitectures.iOS, "armv7-ios"),
    Arm64Darwin("arm64", "darwin", "", "", "lib", ".so", new String[] {"ios", "arm64-ios"}, PlatformArchitectures.iOS, "arm64-ios"),
    Sim64Darwin("sim64", "darwin", "", "", "lib", ".so", new String[] {"ios", "sim64-ios"}, PlatformArchitectures.iOS, "sim64-ios"),
    Armv7Android("armv7", "android", ".so", "lib", "lib", ".so", new String[] {"android", "armv7-android"}, PlatformArchitectures.Android, "armv7-android"),
    JsWeb("js", "web", ".js", "", "lib", "", new String[] {"web", "js-web"}, PlatformArchitectures.Web, "js-web");

    private static HashMap<OS, String> platformPatterns = new HashMap<OS, String>();
    static {
        platformPatterns.put(PlatformProfile.OS.OS_ID_GENERIC, "^$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_WINDOWS, "^x86(_64)?-win32$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_OSX,     "^x86(_64)?-darwin$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_LINUX,   "^x86(_64)?-linux$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_IOS,     "^(arm)|(sim)((v7)|(64))-darwin$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_ANDROID, "^armv7-android$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_WEB,     "^js-web$");
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
    String exeSuffix;
    String exePrefix;
    String libSuffix;
    String libPrefix;
    String[] extenderPaths = null;
    PlatformArchitectures architectures;
    String extenderPair;
    Platform(String arch, String os, String exeSuffix, String exePrefix, String libPrefix, String libSuffix, String[] extenderPaths, PlatformArchitectures architectures, String extenderPair) {
        this.arch = arch;
        this.os = os;
        this.exeSuffix = exeSuffix;
        this.exePrefix = exePrefix;
        this.libSuffix = libSuffix;
        this.libPrefix = libPrefix;
        this.extenderPaths = extenderPaths;
        this.architectures = architectures;
        this.extenderPair = extenderPair;
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

    public String[] getExtenderPaths() {
        return extenderPaths;
    }

    public String getPair() {
        return String.format("%s-%s", this.arch, this.os);
    }

    public String getExtenderPair() {
        return extenderPair;
    }

    public PlatformArchitectures getArchitectures() {
        return architectures;
    }

    public String formatBinaryName(String basename) {
        return exePrefix + basename + exeSuffix;
    }

    public String formatLibraryName(String basename) {
        return libPrefix + basename + libSuffix;
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

    public static Platform getJavaPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Win32;
            }
            else {
                return Platform.X86Win32;
            }
        } else if (os_name.indexOf("mac") != -1) {
            return Platform.X86_64Darwin;
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
        String os_name = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Win32;
            }
            else {
                return Platform.X86Win32;
            }
        } else if (os_name.indexOf("mac") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Darwin;
            } else {
                return Platform.X86Darwin;
            }
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
}
