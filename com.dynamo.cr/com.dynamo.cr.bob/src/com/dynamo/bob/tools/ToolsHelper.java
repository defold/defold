// Copyright 2020-2025 The Defold Foundation
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

package com.dynamo.bob.tools;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;


public class ToolsHelper {
    /**
     * Get file from Java bin folder. Will use java.home property or JAVA_HOME
     * environment variable to get full path
     * @param file Filename to get full path for
     * @return Full path to file
     */
    public static String getJavaBinFile(String file) {
        String javaHome = System.getProperty("java.home");
        if (javaHome == null) {
            javaHome = System.getenv("JAVA_HOME");
        }
        if (javaHome != null) {
            file = Paths.get(javaHome, "bin", file).toString();
        }
        return file;
    }

    /**
     * Extract a file from a zip archive
     * @param zipFile Zip archive to extract from
     * @param fileName File to extract
     * @param outputFile Target file to extract to
     */
    public static void extractFile(File zipFile, String fileName, File outputFile) throws IOException {
        // Wrap the file system in a try-with-resources statement
        // to auto-close it when finished and prevent a memory leak
        try (FileSystem fileSystem = FileSystems.newFileSystem(zipFile.toPath(), new HashMap<>())) {
            Path fileToExtract = fileSystem.getPath(fileName);
            Files.copy(fileToExtract, outputFile.toPath());
        }
    }

}