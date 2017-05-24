package com.defold.editor;

import com.defold.util.SupportPath;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Editor {

    public static boolean isDev() {
        return System.getProperty("defold.version") == null;
    }

    public static Path getSupportPath() {
        try {
            Path supportPath = SupportPath.getPlatformSupportPath("Defold");
            if (supportPath != null && (Files.exists(supportPath) || supportPath.toFile().mkdirs())) {
                return supportPath;
            }
        } catch (Exception e) {
            System.err.println("Unable to determine support path: " + e);
        }

        return null;
    }

    public static Path getLogDirectory() {
        String defoldLogDir = System.getProperty("defold.log.dir");
        Path logDirectory = defoldLogDir != null ? Paths.get(defoldLogDir) : getSupportPath();
        if (logDirectory == null) {
            logDirectory = Paths.get(System.getProperty("user.home"));
        }
        return logDirectory;
    }
}
