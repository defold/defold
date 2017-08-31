package com.defold.editor;


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
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Darwin;
            } else {
                return Platform.X86Darwin;
            }
        } else if (os.indexOf("linux") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return Platform.X86_64Linux;
            } else {
                return Platform.X86Linux;
            }
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
