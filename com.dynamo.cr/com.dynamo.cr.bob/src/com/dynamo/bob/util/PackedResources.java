package com.dynamo.bob.util;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;

import java.io.File;
import java.io.IOException;
import java.net.JarURLConnection;
import java.net.URL;
import java.util.jar.JarFile;

public class PackedResources {
    // Async unpack state and synchronization
    private static volatile boolean unpackStarted = false;
    private static volatile boolean unpackDone = false;
    private static final Object unpackLock = new Object();
    private static JarFile jarFile;

    private static void runUnpackAllLibsAsync(Platform platform) throws IOException {
        TimeProfiler.start("runUnpackAllLibsAsync");
        String platformPair = platform.getPair();
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
                jarFile = jarConnection.getJarFile();
                String basePath = "libexec/" + platformPair + "/";
                String luaZip = "lib/luajit-share.zip";
                jarFile.stream().forEach(entry -> {
                    String name = entry.getName();
                    if (!entry.isDirectory()) {
                        if(name.startsWith(basePath) && !name.contains("dmengine")) {
                            String relativeName = name.substring(basePath.length());
                            File targetFile = new File(targetDir, relativeName);
                            try {
                                URL resourceUrl = Bob.class.getResource("/" + name);
                                if (resourceUrl != null) {
                                    Bob.atomicCopy(resourceUrl, targetFile, true);
                                }
                            } catch (IOException e) {
                                throw new RuntimeException("Failed to copy tool: " + name, e);
                            }
                        }
                        else if (name.startsWith(luaZip)) {
                            try {
                                Bob.extract(Bob.class.getResource("/lib/luajit-share.zip"), new File(Bob.getRootFolder(), "share"));
                            } catch (IOException e) {
                                throw new RuntimeException("Failed to extract: " + name, e);
                            }
                        }
                    }
                });
            } catch (IOException e) {
                throw new IOException("Failed to unpack tools from /libexec/" + platformPair, e);
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
                            throw new RuntimeException("Failed to copy tool: " + file.getName(), e);
                        }
                    }
                }
            }
            // Extract luajit-share.zip if present in lib directory
            File libDir = new File(libRoot.getPath());
            File luajitZip = new File(libDir, "luajit-share.zip");
            if (luajitZip.exists()) {
                try {
                    Bob.extract(luajitZip.toURI().toURL(), new File(Bob.getRootFolder(), "share"));
                } catch (IOException e) {
                    throw new RuntimeException("Failed to extract: " + luajitZip.getAbsolutePath(), e);
                }
            }
        } else {
            throw new IOException("Unsupported protocol for /libexec/" + platformPair + ": " + libexecRoot.getProtocol());
        }
        TimeProfiler.stop();
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
        }
        Thread unpackThread = new Thread(() -> {
            try {
                runUnpackAllLibsAsync(platform);
            } catch (IOException e) {
                throw new RuntimeException("Failed to unpack tools", e);
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
        }
    }

    public static void reset() throws IOException {
        unpackStarted = false;
        unpackDone = false;
        if (jarFile != null) {
            jarFile.close();
        }
    }
}
