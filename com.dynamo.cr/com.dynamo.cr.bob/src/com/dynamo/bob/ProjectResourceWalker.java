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

package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.stream.Collectors;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.TimeProfiler;

// Owns project-local resource walking, ignore rules, and the cached resource path list.
class ProjectResourceWalker {

    private final Project project;
    private final IFileSystem fileSystem;
    private List<String> allResourcePathsCache; // Cache for all resource paths, since Bob doesn't change project files during build
    private List<String> ignoredResourcePathPatterns;

    ProjectResourceWalker(Project project, IFileSystem fileSystem) {
        this.project = project;
        this.fileSystem = fileSystem;
    }

    public void clearCaches() {
        allResourcePathsCache = null;
        ignoredResourcePathPatterns = null;
    }

    public void initIgnorePatterns() throws CompileExceptionError {
        loadIgnoredResourcePathPatterns();
    }

    public void findResourcePathsByExtension(String path, String ext, Collection<String> result) {
        TimeProfiler.start("findResourcePathsByExtension");
        TimeProfiler.addData("path", path);
        TimeProfiler.addData("ext", ext);

        // Initialize and cache all paths only once.
        if (allResourcePathsCache == null) {
            List<String> allPaths = new ArrayList<>();
            walkResources("", new FileSystemWalker() {
                @Override
                public void handleFile(String filePath, Collection<String> results) {
                    results.add(FilenameUtils.normalize(filePath, true));
                }
            }, allPaths);
            allResourcePathsCache = allPaths;
        }

        path = FilenameUtils.normalize(path, true);
        path = Project.stripLeadingSlash(path);

        if ((ext == null || ext.isEmpty()) && (path == null || path.isEmpty())) {
            result.addAll(allResourcePathsCache);
            TimeProfiler.stop();
            return;
        }

        IResource resource = fileSystem.get(path);
        try {
            // Normalize directory lookups so prefix matching stays within that subtree.
            if ((path != null && !path.isEmpty()) && (!resource.isFile() || resource.getContent() == null)) {
                if (!path.endsWith("/")) {
                    path += "/";
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        final String normalizedPath = path;
        // Filter the cached list in parallel, collect safely, then add to result.
        List<String> filteredPaths = allResourcePathsCache.parallelStream()
                .filter(p -> (ext == null || p.endsWith(ext))
                        && (normalizedPath == null || normalizedPath.isEmpty() || p.startsWith(normalizedPath)))
                .collect(Collectors.toList());
        result.addAll(filteredPaths);
        TimeProfiler.stop();
    }

    public void findResourceDirs(String path, Collection<String> result) {
        // Make sure the path has Unix separators, since this is how
        // paths are specified game project relative internally.
        final String normalizedPath = Project.stripLeadingSlash(FilenameUtils.separatorsToUnix(path));
        walkResources(normalizedPath, new FileSystemWalker() {
            @Override
            public boolean handleDirectory(String dir, Collection<String> results) {
                if (normalizedPath.equals(dir)) {
                    return true;
                }
                results.add(FilenameUtils.getName(FilenameUtils.normalizeNoEndSeparator(dir)));
                return false; // skip recursion
            }

            @Override
            public void handleFile(String path, Collection<String> results) {
            }
        }, result);
    }

    // Centralized project walk that applies .defignore filtering before delegating.
    private void walkResources(String path, IFileSystem.IWalker walker, Collection<String> results) {
        fileSystem.walk(path, new FileSystemWalker() {
            @Override
            public boolean handleDirectory(String dir, Collection<String> results) {
                if (isIgnoredResourcePath(dir)) {
                    return false;
                }
                return walker.handleDirectory(dir, results);
            }

            @Override
            public void handleFile(String filePath, Collection<String> results) {
                if (!isIgnoredResourcePath(filePath)) {
                    walker.handleFile(filePath, results);
                }
            }
        }, results);
    }

    /*
        The same `.defignore` matching logic is implemented in the Editor.
        If you change something here, make sure you change it in resource.clj
        (defignore-pred).
    */
    private List<String> loadIgnoredResourcePathPatterns() throws CompileExceptionError {
        if (ignoredResourcePathPatterns != null) {
            return ignoredResourcePathPatterns;
        }

        LinkedHashSet<String> patterns = new LinkedHashSet<>();
        for (String excludeFolder : BundleHelper.createArrayFromString(project.option("exclude-build-folder", ""))) {
            String normalizedPath = normalizeIgnoredPathPattern(excludeFolder);
            if (!normalizedPath.isEmpty()) {
                patterns.add(normalizedPath);
            }
        }

        File defIgnoreFile = new File(project.getRootDirectory(), ".defignore");
        if (defIgnoreFile.isFile()) {
            try {
                for (String entry : FileUtils.readLines(defIgnoreFile, "UTF-8")) {
                    if (!entry.startsWith("/")) {
                        continue;
                    }

                    String normalizedPath = normalizeIgnoredPathPattern(entry);
                    if (!normalizedPath.isEmpty()) {
                        patterns.add(normalizedPath);
                    }
                }
            } catch (IOException e) {
                throw new CompileExceptionError("Unable to read .defignore", e);
            }
        }

        ignoredResourcePathPatterns = new ArrayList<>(patterns);
        return ignoredResourcePathPatterns;
    }

    private List<String> getIgnoredResourcePathPatterns() {
        try {
            return loadIgnoredResourcePathPatterns();
        } catch (CompileExceptionError e) {
            throw new RuntimeException(e);
        }
    }

    private boolean isIgnoredResourcePath(String path) {
        String normalizedPath = normalizeResourcePathForMatching(path);
        if (normalizedPath.isEmpty()) {
            return false;
        }

        for (String ignoredPathPattern : getIgnoredResourcePathPatterns()) {
            if (normalizedPath.equals(ignoredPathPattern) || normalizedPath.startsWith(ignoredPathPattern + "/")) {
                return true;
            }
        }

        return false;
    }

    private static String normalizeIgnoredPathPattern(String path) {
        path = FilenameUtils.separatorsToUnix(path);
        return Project.stripLeadingAndTrailingSlashes(path);
    }

    private static String normalizeResourcePathForMatching(String path) {
        path = FilenameUtils.separatorsToUnix(path);
        if (path.startsWith("./")) {
            path = path.substring(2);
        }
        return Project.stripLeadingAndTrailingSlashes(path);
    }
}
