// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.util;

import com.dynamo.bob.logging.Logger;
import org.apache.commons.io.FileUtils;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;

/**
 * Owns temporary files and directories created during a Bob invocation, keeping them
 * under one root so {@link com.dynamo.bob.Project#dispose()} can remove them
 * together after the build.
 */
public class BobTempScope implements AutoCloseable {

    private static final Logger logger = Logger.getLogger(BobTempScope.class.getName());
    private static final String KEEP_TEMP_ENV = "DM_BOB_KEEP_TEMP";
    private static final String KEEP_TEMP_PROPERTY = "defold.bob.keepTemp";

    private final File rootDirectory;
    private final boolean keepTemp;
    private final Thread shutdownHook;
    private boolean closed = false;

    public BobTempScope() throws IOException {
        this.rootDirectory = Files.createTempDirectory("defold-bob-invoke-").toFile();
        this.keepTemp = shouldKeepTemp();
        this.shutdownHook = new Thread(this::cleanupOnShutdown, "bob-temp-scope-cleanup");
        Runtime.getRuntime().addShutdownHook(this.shutdownHook);
    }

    private static boolean shouldKeepTemp() {
        String propertyValue = System.getProperty(KEEP_TEMP_PROPERTY);
        if (propertyValue != null) {
            return Boolean.parseBoolean(propertyValue);
        }

        String envValue = System.getenv(KEEP_TEMP_ENV);
        return envValue != null && ("1".equals(envValue) || "true".equalsIgnoreCase(envValue));
    }

    public synchronized File createTempFile(String prefix, String suffix) throws IOException {
        checkOpen();
        return Files.createTempFile(rootDirectory.toPath(), prefix, suffix).toFile();
    }

    public synchronized File createTempDirectory(String prefix) throws IOException {
        checkOpen();
        return Files.createTempDirectory(rootDirectory.toPath(), prefix).toFile();
    }

    public File getRootDirectory() {
        return rootDirectory;
    }

    private void checkOpen() throws IOException {
        if (closed) {
            throw new IOException("Bob temporary scope is already closed");
        }
    }

    @Override
    public synchronized void close() {
        if (closed) {
            return;
        }
        closed = true;
        unregisterShutdownHook();
        cleanupRootDirectory();
    }

    private synchronized void cleanupOnShutdown() {
        if (closed) {
            return;
        }
        closed = true;
        cleanupRootDirectory();
    }

    private void unregisterShutdownHook() {
        try {
            Runtime.getRuntime().removeShutdownHook(this.shutdownHook);
        } catch (IllegalStateException ignored) {
            // The JVM is already shutting down, so the hook is either running or about to run.
        }
    }

    private void cleanupRootDirectory() {
        if (keepTemp) {
            logger.info("Keeping Bob temporary directory '%s'", rootDirectory.getAbsolutePath());
            return;
        }

        try {
            FileUtils.deleteDirectory(rootDirectory);
        } catch (IOException e) {
            logger.warning("Failed to delete Bob temporary directory '%s': %s", rootDirectory.getAbsolutePath(), e.getMessage());
        }
    }
}
