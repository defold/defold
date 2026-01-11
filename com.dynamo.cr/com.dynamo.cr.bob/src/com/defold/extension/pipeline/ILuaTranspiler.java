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

package com.defold.extension.pipeline;

import java.io.File;
import java.util.List;
import java.util.Objects;

/**
 * Implement this interface in com.defold.extension.pipeline package with no-argument constructor
 * to add support for transpiling a language to Lua that is then will be used during the build
 */
public interface ILuaTranspiler {

    /**
     * @return A resource path to a build file, i.e. a file that configures the transpiler;
     * a project-relative path starting with slash, e.g. {@code "/tlconfig.lua"}
     */
    String getBuildFileResourcePath();

    /**
     * @return A file extension for the source code files of the transpiled language,
     * e.g. {@code "ext"}
     */
    String getSourceExt();

    /**
     * Build the project from the source dir to output dir
     *
     * @param pluginDir a build plugins dir inside the project root that contains unpacked plugins
     *                  from all project extensions, useful for distributing and using transpiler
     *                  binaries. When directory is a native extension (i.e. has {@code "ext.manifest"}
     *                  file), even if it comes from a dependency, files from
     *                  {@code "${ext-dir}/plugins/bin/${platform}/"} (or {@code "${ext-dir}/plugins/bin/${platform}.zip"})
     *                  will be extracted to the plugin dir, and then may be subsequently accessed (and
     *                  executed) using this path, e.g. with {@code new File(pluginDir, "my-transpiler-lib/plugins/bin/"
     *                  + Platform.getHostPlatform().getPair() + "/my-transpiler.exe")}
     * @param sourceDir a dir that is guaranteed to have all the source code files as reported
     *                  by {@link #getSourceExt()}, and a build file, as reported by
     *                  {@link #getBuildFileResourcePath()}. This might be a real project dir,
     *                  or it could be a temporary directory if some sources are coming from
     *                  the dependency zip files.
     * @param outputDir a dir to put the transpiled lua files to. All lua files from the
     *                  directory will be compiled as a part of the project compilation. The
     *                  directory is preserved over editor/bob transpiler runs and editor restarts,
     *                  so it's a responsibility of the transpiler to clear out unneeded files from
     *                  previous runs
     * @return a possibly empty (but not nullable!) list of build issues
     */
    List<Issue> transpile(File pluginDir, File sourceDir, File outputDir) throws Exception;

    final class Issue {
        public final Severity severity;
        public final String resourcePath;
        public final int lineNumber;
        public final String message;

        /**
         * @param severity     issue severity
         * @param resourcePath compiled resource path of the build file or some of the source
         *                     files relative to project root, must start with slash
         * @param lineNumber   1-indexed line number
         * @param message      issue message to present to the user
         */
        public Issue(Severity severity, String resourcePath, int lineNumber, String message) {
            if (severity == null) {
                throw new IllegalArgumentException("Issue severity must not be null");
            }
            if (resourcePath == null || resourcePath.isEmpty() || resourcePath.charAt(0) != '/') {
                throw new IllegalArgumentException("Invalid issue resource path: " + resourcePath);
            }
            if (lineNumber <= 0) {
                throw new IllegalArgumentException("Invalid issue line number: " + lineNumber);
            }
            if (message == null || message.isEmpty()) {
                throw new IllegalArgumentException("Invalid issue message: " + message);
            }
            this.severity = severity;
            this.resourcePath = resourcePath;
            this.lineNumber = lineNumber;
            this.message = message;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            Issue issue = (Issue) o;
            return lineNumber == issue.lineNumber && severity == issue.severity && Objects.equals(resourcePath, issue.resourcePath) && Objects.equals(message, issue.message);
        }

        @Override
        public int hashCode() {
            return Objects.hash(severity, resourcePath, lineNumber, message);
        }

        @Override
        public String toString() {
            return "Issue{" +
                    "severity=" + severity +
                    ", resourcePath='" + resourcePath + '\'' +
                    ", lineNumber=" + lineNumber +
                    ", message='" + message + '\'' +
                    '}';
        }
    }

    enum Severity {INFO, WARNING, ERROR}
}
