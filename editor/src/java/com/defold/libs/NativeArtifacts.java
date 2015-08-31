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

public class NativeArtifacts {

    private static Logger logger = LoggerFactory.getLogger(NativeArtifacts.class);
    private static File libPath;
    private static File exePath;
    private static boolean isInitialized = false;

    private static Map<com.defold.editor.Platform, String[]> nativeLibs = new HashMap<>();
    static {
        nativeLibs.put(Platform.X86_64Darwin, new String[] {
                "libjogl_desktop.jnilib", "libjogl_mobile.jnilib",
                "libnativewindow_awt.jnilib", "libnativewindow_macosx.jnilib",
                "libnewt.jnilib", "libgluegen-rt.jnilib",
                "libparticle_shared.dylib", "libtexc_shared.dylib",
                "libjnidispatch.jnilib"});

        nativeLibs.put(Platform.X86Win32, new String[] { "jogl_desktop.dll",
                "jogl_mobile.dll", "nativewindow_awt.dll",
                "nativewindow_win32.dll", "newt.dll", "gluegen-rt.dll",
                "particle_shared.dll", "texc_shared.dll",
                "jnidispatch.dll"});

        nativeLibs.put(Platform.X86Linux, new String[] { "libjogl_desktop.so",
                "libjogl_mobile.so", "libnativewindow_awt.so",
                "libnativewindow_x11.so", "libnewt.so", "libgluegen-rt.so",
                "libparticle_shared.so", "libtexc_shared.so",
                "libjnidispatch.so"});

        nativeLibs.put(Platform.X86_64Linux, nativeLibs.get(Platform.X86Linux));
        nativeLibs.put(Platform.X86_64Win32, nativeLibs.get(Platform.X86Win32));
    }

    private static Map<com.defold.editor.Platform, String[]> nativeExes = new HashMap<>();
    static {
        nativeExes.put(Platform.X86_64Darwin, new String[] { "dmengine", "dmengine_release" });
        nativeExes.put(Platform.X86Win32, new String[] { "dmengine.exe", "dmengine_release.exe" });
        nativeExes.put(Platform.X86Linux, new String[] { "dmengine", "dmengine_release" });
        nativeExes.put(Platform.X86_64Linux, nativeExes.get(Platform.X86Linux));
        nativeExes.put(Platform.X86_64Win32, nativeExes.get(Platform.X86Win32));
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

    private static void extractNativeExe(Platform platform, String name) throws IOException {
        String path = String.format("/libexec/%s/%s", platform.getPair(), name);
        URL resource = logger.getClass().getResource(path);
        if (resource != null) {
            logger.debug("extracting exe {}", path);
            File destination = new File(exePath, new File(path).getName());
			FileUtils.copyURLToFile(resource, destination);
			destination.setExecutable(true);
        } else {
            logger.warn("can't find exe {}", path);
        }
    }

    public static void extractNatives() throws IOException {
        if (isInitialized) {
            return;
        }

        isInitialized = true;

        if (Editor.isDev()) {
            String dynamo_home = System.getenv("DYNAMO_HOME");
            libPath = new File(String.format("%s/lib/%s", dynamo_home, Platform.getJavaPlatform().getPair()));
            exePath = new File(String.format("%s/bin/%s", dynamo_home, Platform.getHostPlatform().getPair()));
        } else {
            libPath = Files.createTempDirectory(null).toFile();
            exePath = Files.createTempDirectory(null).toFile();
        }

        logger.debug("defold.exe.path={}", exePath);
        logger.debug("java.library.path={}", libPath);

        if (!Editor.isDev()) {
            Platform platform = Platform.getJavaPlatform();
            for (String name : nativeLibs.get(platform)) {
                extractNativeLib(platform, name);
            }
            for (String name : nativeExes.get(platform)) {
                extractNativeExe(platform, name);
            }
            System.setProperty("jogamp.gluegen.UseTempJarCache", "false");
            System.setProperty("jna.boot.library.path", libPath.getAbsolutePath());
        }
        System.setProperty("defold.exe.path", exePath.getAbsolutePath());
        System.setProperty("java.library.path", libPath.getAbsolutePath());
        System.setProperty("jna.library.path", libPath.getAbsolutePath());
    }

}
