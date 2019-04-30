package com.dynamo.bob;

public enum PlatformArchitectures {
    OSX(new String[] {"x86_64-darwin"}, new String[] {"x86_64-darwin"}),
    Windows(new String[] {"x86_64-win32", "x86-win32"}, new String[] {"x86_64-win32", "x86-win32"}),
    Linux(new String[] {"x86_64-linux"}, new String[] {"x86_64-linux"}),
    iOS(new String[] {"arm64-darwin", "armv7-darwin", "x86_64-ios"}, new String[] {"arm64-darwin", "armv7-darwin"}),
    Android(new String[] {"arm64-android", "armv7-android"}, new String[] {"armv7-android"}),
    Web(new String[] {"js-web", "wasm-web"}, new String[] {"js-web", "wasm-web"});

    String[] architectures;
    String[] defaultArchitectures;
    PlatformArchitectures(String[] architectures, String[] defaultArchitectures) {
        this.architectures = architectures;
        this.defaultArchitectures = defaultArchitectures;
    }

    String[] getArchitectures() {
        return architectures;
    }

    String[] getDefaultArchitectures() {
        return defaultArchitectures;
    }
}
