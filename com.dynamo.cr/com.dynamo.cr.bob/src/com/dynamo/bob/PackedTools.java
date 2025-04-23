package com.dynamo.bob;

import com.dynamo.bob.util.TimeProfiler;

import java.io.File;
import java.io.IOException;
import java.net.JarURLConnection;
import java.net.URL;

public class PackedTools {
    // Async unpack state and synchronization
    private static volatile boolean unpackStarted = false;
    private static volatile boolean unpackDone = false;
    private static final Object unpackLock = new Object();
    /**
     * Unpack all tools for a given platform from the /libexec/<platform> directory in the JAR to the root folder.
     * Excludes files containing "dmengine" in their name.
     */
    private static void unpackAll(Platform platform) throws IOException {
        TimeProfiler.start("UnpackAllTools");
        String platformPair = platform.getPair();
        File targetDir = new File(Bob.getRootFolder(), platformPair);

        // Ensure the target directory exists
        if (!targetDir.exists()) {
            targetDir.mkdirs();
        }

        // List all resources in the /libexec/<platform> directory
        URL libexecRoot = Bob.class.getResource("/libexec/" + platformPair);
        if (libexecRoot == null) {
            throw new IOException("Could not find /libexec/" + platformPair);
        }

        // The resource might be in a JAR, so we extract entries from the JAR
        try {
            JarURLConnection jarConnection = (JarURLConnection) new URL(libexecRoot.toString()).openConnection();
            try (var jarFile = jarConnection.getJarFile()) {
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
            }
        } catch (IOException e) {
            throw new IOException("Failed to unpack tools from /libexec/" + platformPair, e);
        }
        TimeProfiler.stop();
    }

    /**
     * Run unpackAll asynchronously. Only starts once.
     */
    public static void runUnpackAllAsync(Platform platform) {
        synchronized (unpackLock) {
            if (unpackStarted) {
                return;
            }
            unpackStarted = true;
        }
        Thread unpackThread = new Thread(() -> {
            try {
                unpackAll(platform);
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
     * Wait for the completion of async unpackAll.
     */
    public static void waitForUnpackAll() {
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

    public static void reset() {
        unpackStarted = false;
        unpackDone = false;
    }
}
