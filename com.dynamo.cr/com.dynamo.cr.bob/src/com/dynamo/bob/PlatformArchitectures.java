package com.dynamo.bob;

public enum PlatformArchitectures {
    OSX(new String[] {"x86_64-darwin"}),
    Windows(new String[] {"x86_64-win32", "x86-win32"}),
    Linux(new String[] {"x86_64-linux"}),
    iOS(new String[] {"arm64-darwin", "armv7-darwin"}),
    Android(new String[] {"armv7-android"}),
    Web(new String[] {"js-web"});

    String[] architectures;
    PlatformArchitectures(String[] architectures) {
        this.architectures = architectures;
    }

    String[] getArchitectures() {
        return architectures;
    }
}
