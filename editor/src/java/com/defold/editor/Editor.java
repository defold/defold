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
        Path supportPath = SupportPath.getPlatformSupportPath("Defold");
        if (!Files.exists(supportPath)) {
            supportPath.toFile().mkdirs();
        }
        return supportPath;
    }

    public static Path getLogDirectory() {
        String defoldLogDir = System.getProperty("defold.log.dir");
        return defoldLogDir != null ? Paths.get(defoldLogDir) : getSupportPath();
    }
}
