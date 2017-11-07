package com.defold.util;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

import com.defold.editor.Platform;

public class SupportPath {

    public static Path getPlatformSupportPath(String applicationName) {
        Platform platform = Platform.getHostPlatform();
        switch(platform.getOs()) {
            case "win32":
                return getWindowsSupportPath(applicationName);
            case "darwin":
                return getMacSupportPath(applicationName);
            case "linux":
                return getLinuxSupportPath(applicationName);
            default:
                return null;
        }
    }

    // macOS

    public static Path getMacSupportPath(String applicationName) {
        final String userHome = System.getProperty("user.home");
        if (userHome == null) {
            return null;
        }
        Path applicationSupport = Paths.get(userHome, "Library", "Application Support");
        if (Files.isDirectory(applicationSupport)) {
            return applicationSupport.resolve(applicationName);
        }

        return null;
    }

    // win

    public static Path getWindowsSupportPath(String applicationName) {
        // 1. LOCALAPPDATA environment variable
        final String localAppDataEnv = System.getenv("LOCALAPPDATA");
        if (localAppDataEnv != null) {
            Path localAppData = Paths.get(localAppDataEnv);
            if (Files.isDirectory(localAppData)) {
                return localAppData.resolve(applicationName);
            }
        }

        // Ensure we have a user.home set
        final String userHome = System.getProperty("user.home");
        if (userHome == null) {
            return null;
        }

        // 2. "${user.home}/AppData/Local"
        final Path appDataLocal = Paths.get(userHome, "AppData", "Local");
        if (Files.isDirectory(appDataLocal)) {
            return appDataLocal.resolve(applicationName);
        }

        // 3. "${user.home}/Local Settings/Application Data"
        final Path localSettingsAppData = Paths.get(userHome, "Local Settings", "Application Data");
        if (Files.isDirectory(localSettingsAppData)) {
            return localSettingsAppData.resolve(applicationName);
        }

        return null;
    }

    // linux

    private static Path getLinuxSupportPath(String applicationName) {
        final String userHome = System.getProperty("user.home");
        if (userHome == null) {
            return null;
        }
        final Path home = Paths.get(userHome);
        if (Files.isDirectory(home)) {
            return home.resolve("." + applicationName);
        }

        return null;
    }

    public static void main(String[] args) {
        System.out.println("done: " + getPlatformSupportPath("Defold"));
    }
}
