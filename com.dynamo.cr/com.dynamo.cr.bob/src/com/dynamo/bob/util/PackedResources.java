package com.dynamo.bob.util;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.ModelImporterJni;
import com.dynamo.bob.pipeline.ShadercJni;
import com.dynamo.bob.pipeline.TexcLibraryJni;
import org.apache.commons.io.FileUtils;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.reflect.InvocationTargetException;
import java.net.JarURLConnection;
import java.net.URL;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicReference;
import java.util.jar.JarFile;

public class PackedResources {
    private static final String TOOLS_LOCK = ".bob-tools.lock";
    private static final String TOOLS_COMPLETE = ".bob-tools-%s-complete";
    private static final String LUAJIT_SHARE_ZIP = "lib/luajit-share.zip";
    private static final String LUAJIT_SHARE_DIR = "luajit";

    // Async unpack state and synchronization
    private static volatile boolean unpackStarted = false;
    private static volatile boolean unpackDone = false;
    private static final Object unpackLock = new Object();
    private static final AtomicReference<Throwable> unpackError = new AtomicReference<>();
    private static final Class<?>[] NATIVE_LIB_CLASSES = new Class<?>[] {
            TexcLibraryJni.class,
            ShadercJni.class,
            ModelImporterJni.class
    };

    private static void loadNativeLib(Class<?> libClass) {
        TimeProfiler.start("loadNativeLib %s", libClass.getSimpleName());
        try {
            libClass.getDeclaredConstructor().newInstance();
        } catch (InvocationTargetException e) {
            Throwable cause = e.getCause() != null ? e.getCause() : e;
            throw new RuntimeException("Failed to load native library: " + libClass.getSimpleName(), cause);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException("Failed to load native library: " + libClass.getSimpleName(), e);
        } finally {
            TimeProfiler.stop();
        }
    }

    private static Thread[] startNativeLibLoads(AtomicReference<Throwable> nativeLibError) {
        Thread[] threads = new Thread[NATIVE_LIB_CLASSES.length];
        for (int i = 0; i < NATIVE_LIB_CLASSES.length; i++) {
            final Class<?> libClass = NATIVE_LIB_CLASSES[i];
            threads[i] = new Thread(new Runnable() {
                @Override
                public void run() {
                    try {
                        loadNativeLib(libClass);
                    } catch (Throwable t) {
                        nativeLibError.compareAndSet(null, t);
                    }
                }
            });
            threads[i].start();
        }
        return threads;
    }

    private static void awaitNativeLibLoads(Thread[] threads, AtomicReference<Throwable> nativeLibError) {
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new RuntimeException("Interrupted while loading native libraries", e);
            }
        }

        Throwable loadError = nativeLibError.get();
        if (loadError != null) {
            throw new RuntimeException("Failed to load native libraries", loadError);
        }
    }

    private static File resolvePackedResourceTarget(File targetDir, String relativeName) throws IOException {
        File canonicalTargetDir = targetDir.getCanonicalFile();
        File targetFile = new File(canonicalTargetDir, relativeName).getCanonicalFile();
        if (!targetFile.toPath().startsWith(canonicalTargetDir.toPath())) {
            throw new IOException(String.format("Packed resource '%s' resolves outside of '%s'", relativeName, canonicalTargetDir.getAbsolutePath()));
        }
        return targetFile;
    }

    private static File getCompletionMarker(File rootFolder, String platformPair) {
        return new File(rootFolder, String.format(TOOLS_COMPLETE, platformPair));
    }

    private static void atomicExtractLuaJITShare(URL luajitZip, File rootFolder) throws IOException {
        File shareDir = new File(rootFolder, "share");
        File luajitDir = new File(shareDir, LUAJIT_SHARE_DIR);
        if (luajitDir.exists()) {
            FileUtils.deleteDirectory(luajitDir);
        }
        Bob.atomicExtractDirectory(luajitZip, shareDir, LUAJIT_SHARE_DIR);
    }

    private static void unpackPlatformResources(Platform platform) throws IOException {
        String platformPair = platform.getPair();
        File rootFolder = Bob.getRootFolder();
        File targetDir = new File(Bob.getRootFolder(), platformPair);

        // Ensure the target directory exists
        if (!targetDir.exists()) {
            targetDir.mkdirs();
        }

        // List all resources in the /libexec/<platform> directory
        URL libexecRoot = Bob.class.getResource("/libexec/" + platformPair);
        URL libRoot = Bob.class.getResource("/lib/");
        if (libexecRoot == null) {
            throw new IOException("Could not find /libexec/" + platformPair);
        }
        if (libRoot == null) {
            throw new IOException("Could not find /lib/");
        }

        // The resource might be in a JAR, or a directory in the file system
        if (libexecRoot.getProtocol().equals("jar")) {
            try {
                JarURLConnection jarConnection = (JarURLConnection) libexecRoot.openConnection();
                jarConnection.setUseCaches(false);
                try (JarFile jarFile = jarConnection.getJarFile()) {
                    String basePath = "libexec/" + platformPair + "/";
                    jarFile.stream().forEach(entry -> {
                        String name = entry.getName();
                        if (!entry.isDirectory()) {
                            if(name.startsWith(basePath) && !name.contains("dmengine")) {
                                String relativeName = name.substring(basePath.length());
                                try {
                                    File targetFile = resolvePackedResourceTarget(targetDir, relativeName);
                                    URL resourceUrl = Bob.class.getResource("/" + name);
                                    if (resourceUrl != null) {
                                        Bob.atomicCopy(resourceUrl, targetFile, true);
                                    }
                                } catch (IOException e) {
                                    throw new RuntimeException(String.format("Failed to copy packed tool '%s'", name), e);
                                }
                            }
                            else if (name.equals(LUAJIT_SHARE_ZIP)) {
                                try {
                                    atomicExtractLuaJITShare(Bob.class.getResource("/" + LUAJIT_SHARE_ZIP), rootFolder);
                                } catch (IOException e) {
                                    throw new RuntimeException("Failed to extract packed LuaJIT share archive: " + name, e);
                                }
                            }
                        }
                    });
                }
            } catch (IOException e) {
                throw new IOException(String.format("Failed to unpack packed tools from /libexec/%s into '%s'", platformPair, targetDir.getAbsolutePath()), e);
            }
        } else if ("file".equals(libexecRoot.getProtocol())) {
            // Directory in file system case (e.g. during development)
            File dir = new File(libexecRoot.getPath());
            // Copy tools (excluding dmengine)
            File[] files = dir.listFiles((d, name) -> !name.contains("dmengine"));
            if (files != null) {
                for (File file : files) {
                    if (file.isFile()) {
                        File targetFile = new File(targetDir, file.getName());
                        try {
                            Bob.atomicCopy(file.toURI().toURL(), targetFile, true);
                        } catch (IOException e) {
                            throw new RuntimeException(String.format("Failed to copy packed tool '%s' to '%s'", file.getName(), targetFile.getAbsolutePath()), e);
                        }
                    }
                }
            }
            // Extract luajit-share.zip if present in lib directory
            File libDir = new File(libRoot.getPath());
            File luajitZip = new File(libDir, "luajit-share.zip");
            if (luajitZip.exists()) {
                try {
                    atomicExtractLuaJITShare(luajitZip.toURI().toURL(), rootFolder);
                } catch (IOException e) {
                    throw new RuntimeException("Failed to extract packed LuaJIT share archive: " + luajitZip.getAbsolutePath(), e);
                }
            }
        } else {
            throw new IOException("Unsupported protocol for /libexec/" + platformPair + ": " + libexecRoot.getProtocol());
        }
    }

    private static void runUnpackAllLibsAsync(Platform platform) throws IOException {
        TimeProfiler.start("runUnpackAllLibsAsync");
        try {
            File rootFolder = Bob.getRootFolder();
            String platformPair = platform.getPair();
            File lockFile = new File(rootFolder, TOOLS_LOCK);
            File completionMarker = getCompletionMarker(rootFolder, platformPair);
            lockFile.getParentFile().mkdirs();

            try (RandomAccessFile lockAccess = new RandomAccessFile(lockFile, "rw");
                 FileChannel lockChannel = lockAccess.getChannel();
                 FileLock ignored = lockChannel.lock()) {
                if (completionMarker.exists()) {
                    return;
                }

                unpackPlatformResources(platform);
                Files.write(completionMarker.toPath(), "ok\n".getBytes(StandardCharsets.UTF_8));
            }
        } finally {
            TimeProfiler.stop();
        }
    }

    /**
     * Run unpackAllLibsAsync asynchronously. Only starts once.
     */
    public static void unpackAllLibsAsync(Platform platform) {
        synchronized (unpackLock) {
            if (unpackStarted) {
                return;
            }
            unpackStarted = true;
            unpackError.set(null);
        }
        Thread unpackThread = new Thread(() -> {
            try {
                runUnpackAllLibsAsync(platform);
                AtomicReference<Throwable> nativeLibError = new AtomicReference<>();
                Thread[] nativeLibThreads = startNativeLibLoads(nativeLibError);
                awaitNativeLibLoads(nativeLibThreads, nativeLibError);
            } catch (Throwable t) {
                unpackError.compareAndSet(null, t);
            } finally {
                synchronized (unpackLock) {
                    unpackDone = true;
                    unpackLock.notifyAll();
                }
            }
        });
        unpackThread.start();
    }

    /**
     * Wait for the completion of async unpackAllLibsAsync.
     */
    public static void waitForuUpackAllLibsAsync() {
        synchronized (unpackLock) {
            if (!unpackStarted) {
                return;
            }
            while (!unpackDone) {
                try {
                    unpackLock.wait();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException("Interrupted while waiting for tool unpack", e);
                }
            }
            Throwable error = unpackError.get();
            if (error != null) {
                throw new RuntimeException("Failed to unpack bundled Bob tools", error);
            }
        }
    }

    public static void reset() throws IOException {
        unpackStarted = false;
        unpackDone = false;
        unpackError.set(null);
    }
}
