package com.defold.libs;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.defold.editor.Editor;
import com.defold.editor.Platform;

public class Libs {

    private static Logger logger = LoggerFactory.getLogger(Libs.class);
    private static File libPath;
    private static boolean isInitialized = false;

    private static Map<com.defold.editor.Platform, String[]> nativeLibs = new HashMap<>();
    static {
        nativeLibs.put(Platform.X86_64Darwin, new String[] {
                "libjogl_desktop.jnilib", "libjogl_mobile.jnilib",
                "libnativewindow_awt.jnilib", "libnativewindow_macosx.jnilib",
                "libnewt.jnilib", "libgluegen-rt.jnilib" });

        nativeLibs.put(Platform.X86Win32, new String[] { "jogl_desktop.dll",
                "jogl_mobile.dll", "nativewindow_awt.dll",
                "nativewindow_win32.dll", "newt.dll", "gluegen-rt.dll" });

        nativeLibs.put(Platform.X86Linux, new String[] { "libjogl_desktop.so",
                "libjogl_mobile.so", "libnativewindow_awt.so",
                "libnativewindow_x11.so", "libnewt.so", "libgluegen-rt.so" });

        nativeLibs.put(Platform.X86_64Linux, nativeLibs.get(Platform.X86Linux));
        nativeLibs.put(Platform.X86_64Win32, nativeLibs.get(Platform.X86Win32));
    }

    private static void extractNativeLib(Platform platform, String name) throws IOException {
        String path = String.format("/lib/%s/%s", platform.getPair(), name);
        URL resource = logger.getClass().getResource(path);
        if (resource != null) {
            logger.debug("extracting lib {}", path);
            FileUtils.copyURLToFile(resource,
                    new File(libPath, new File(path).getName()));
        } else {
            logger.warn("can't find library {}", path);
        }
    }

    public static void extractNativeLibs() throws IOException {
        if (isInitialized) {
            return;
        }

        isInitialized = true;

        if (Editor.isDev()) {
            String dynamo_home = System.getenv("DYNAMO_HOME");
            libPath = new File(String.format("%s/lib/%s", dynamo_home, Platform.getJavaPlatform().getPair()));
        } else {
            libPath = Files.createTempDirectory(null).toFile();
        }

        logger.debug("java.library.path={}", libPath);

        if (!Editor.isDev()) {
            Platform platform = Platform.getJavaPlatform();
            for (String name : nativeLibs.get(platform)) {
                extractNativeLib(platform, name);
            }
            System.setProperty("jogamp.gluegen.UseTempJarCache", "false");
        }
        System.setProperty("java.library.path", libPath.getAbsolutePath());
        System.setProperty("jna.library.path", libPath.getAbsolutePath());
    }

}
