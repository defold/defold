package com.dynamo.bob;

public enum PlatformPairs {
    OSX(new String[] {"x86_64-darwin", "x86-darwin"}),
    Windows(new String[] {"x86_64-win32", "x86-win32"}),
    Linux(new String[] {"x86_64-linux", "x86-linux"}),
    iOS(new String[] {"arm64-darwin", "armv7-darwin"}),
    Android(new String[] {"armv7-android"}),
    Web(new String[] {"js-web"});
    
    String[] platforms;
    PlatformPairs(String[] platforms) {
        this.platforms = platforms;
    }
    
    String[] getPlatforms() {
        return platforms;
    }
}
