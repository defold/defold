package com.dynamo.bob;

import java.util.HashMap;

import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;


public enum Platform {
    X86Darwin("x86", "darwin", "", "", "lib", ".dylib"),
    X86_64Darwin("x86_64", "darwin", "", "", "lib", ".dylib"),
    X86Win32("x86", "win32", ".exe", "", "", ".dll"),
    X86_64Win32("x86_64", "win32", ".exe", "", "", ".dll"),
    X86Linux("x86", "linux", "", "", "lib", ".so"),
    X86_64Linux("x86_64", "linux", "", "", "lib", ".so"),
    Armv7Darwin("armv7", "darwin", "", "", "lib", ".so"),
    Arm64Darwin("arm64", "darwin", "", "", "lib", ".so"),
    Armv7Android("armv7", "android", ".so", "lib", "lib", ".so"),
    JsWeb("js", "web", ".js", "", "lib", "");

    private static HashMap<OS, String> platformPatterns = new HashMap<OS, String>();
    static {
        platformPatterns.put(PlatformProfile.OS.OS_ID_GENERIC, "^$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_WINDOWS, "^x86(_64)?-win32$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_OSX,     "^x86(_64)?-darwin$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_LINUX,   "^x86(_64)?-linux$");
        platformPatterns.put(PlatformProfile.OS.OS_ID_IOS,     "^arm((v7)|(64))-darwin$");
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
    Platform(String arch, String os, String exeSuffix, String exePrefix, String libPrefix, String libSuffix) {
        this.arch = arch;
        this.os = os;
        this.exeSuffix = exeSuffix;
        this.exePrefix = exePrefix;
        this.libSuffix = libSuffix;
        this.libPrefix = libPrefix;
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
            return Platform.X86Darwin;
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
