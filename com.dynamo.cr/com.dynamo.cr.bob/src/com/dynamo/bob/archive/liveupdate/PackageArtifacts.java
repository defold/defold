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

package com.dynamo.bob.archive.liveupdate;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BooleanSupplier;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;

/** Tracks generated package paths for bundling, cleanup, and resource discovery. */
public final class PackageArtifacts {

    /**
     * A finalized artifact. path is the destination; name is relative to the package output root.
     * bundle controls copying into the application bundle; legacy retains the old workflow's placement rules.
     */
    public record Output(Path path,
            String name,
            boolean directory,
            boolean bundle,
            boolean legacy) {}

    /** Returns the persistent ownership directory, outside the disposable build directory. */
    private static Path directory(Path projectRoot) {
        return projectRoot.toAbsolutePath().normalize().resolve(".internal/liveupdate");
    }

    /** Locates the ownership index for this build directory and game.project resource. */
    static Path buildIndex(Project project) {
        String identity = project.getBuildDirectory() + "/" + project.getGameProjectResource().getPath();
        String filename = "build-" + Long.toUnsignedString(MurmurHash.hash64(identity), 16) + ".outputs";
        return directory(Path.of(project.getRootDirectory())).resolve(filename);
    }

    /** Locates the ownership index for copies delivered to a particular application bundle directory. */
    static Path bundleIndex(Project project, Path bundleDirectory) {
        Path root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        Path destination = bundleDirectory.toAbsolutePath().normalize();
        String identity = destination.startsWith(root) ? root.relativize(destination).toString() : destination.toString();
        String filename = "bundle-" + Long.toUnsignedString(MurmurHash.hash64(identity), 16) + ".outputs";
        return directory(Path.of(project.getRootDirectory())).resolve(filename);
    }

    /** Reads a persistent index, or returns an empty list when it does not exist. */
    static List<Output> read(Path projectRoot, Path index) throws IOException {
        if (!Files.exists(index)) {
            return List.of();
        }
        return parse(projectRoot, Files.readString(index, StandardCharsets.UTF_8));
    }

    /** Returns the .lupackages build output recording the artifacts from the latest successful build. */
    public static IResource resource(Project project) {
        return project.getGameProjectResource().disableMinifyPath().changeExt(".lupackages");
    }

    /** Serializes artifact paths and placement flags as UTF-8, including the originating project root. */
    public static byte[] serializeOutputs(Project project, List<Output> outputs) {
        StringBuilder text = new StringBuilder(header(project));
        for (Output output : outputs) {
            byte[] path = output.path().toString().getBytes(StandardCharsets.UTF_8);
            byte[] name = output.name().getBytes(StandardCharsets.UTF_8);
            text.append(output.directory()).append(' ')
                    .append(output.bundle()).append(' ')
                    .append(Base64.getEncoder().encodeToString(path)).append(' ')
                    .append(Base64.getEncoder().encodeToString(name)).append(' ')
                    .append(output.legacy()).append('\n');
        }
        return text.toString().getBytes(StandardCharsets.UTF_8);
    }

    /** Reads the latest build-output index; a missing index yields an empty list. */
    public static List<Output> readOutputs(Project project) throws IOException {
        IResource index = resource(project);
        if (!index.exists()) {
            return List.of();
        }
        return parse(Path.of(project.getRootDirectory()), new String(index.getContent(), StandardCharsets.UTF_8));
    }

    /** Records the absolute project root so ownership can be adjusted when a project is copied or moved. */
    private static String header(Project project) {
        String root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize().toString();
        return "project-root " + Base64.getEncoder().encodeToString(root.getBytes(StandardCharsets.UTF_8)) + "\n";
    }

    /**
     * Reads artifact ownership, rebasing project-local artifacts to the supplied project root.
     * External artifacts remain owned only by the originating project.
     */
    static List<Output> parse(Path projectRoot, String content) throws IOException {
        projectRoot = projectRoot.toAbsolutePath().normalize();
        int end = content.indexOf('\n');
        if (!content.startsWith("project-root ") || end < 13) {
            throw new IOException("Invalid Live Update artifact index");
        }
        Path origin = Path.of(new String(Base64.getDecoder().decode(content.substring(13, end)), StandardCharsets.UTF_8));
        content = content.substring(end + 1);
        List<Output> outputs = new ArrayList<>();
        for (String line : content.lines().toList()) {
            String[] fields = line.split(" ", -1);
            if (fields.length < 5 || fields.length > 7) {
                throw new IOException("Invalid Live Update artifact index");
            }
            Path path = Path.of(new String(Base64.getDecoder().decode(fields[2]), StandardCharsets.UTF_8));
            if (path.startsWith(origin)) {
                // A copied or moved project owns its own copies, never the original project's artifacts.
                path = projectRoot.resolve(origin.relativize(path));
            } else if (!projectRoot.equals(origin)) {
                // External paths are owned only by the project that wrote the index.
                continue;
            }
            boolean directory = Boolean.parseBoolean(fields[0]);
            boolean bundle = Boolean.parseBoolean(fields[1]);
            String name = new String(Base64.getDecoder().decode(fields[3]), StandardCharsets.UTF_8);
            // Ignore fingerprints stored by earlier versions of these build records.
            boolean legacy = Boolean.parseBoolean(fields[fields.length - 1]);
            outputs.add(new Output(path, name, directory, bundle, legacy));
        }
        return List.copyOf(outputs);
    }

    /** Reads persistent build ownership, falling back to the latest build-output index when absent. */
    static List<Output> readBuildOutputs(Project project) throws IOException {
        Path index = buildIndex(project);
        return Files.exists(index) ? read(Path.of(project.getRootDirectory()), index) : readOutputs(project);
    }

    /** Retain generated report paths when later builds omit the report options. */
    public static void recordReportOutputs(Project project, Collection<Path> paths) throws IOException {
        Path buildIndex = buildIndex(project);
        Path index = buildIndex.resolveSibling("reports-" + buildIndex.getFileName());
        Map<Path, Output> outputs = new LinkedHashMap<>();
        for (Output output : read(Path.of(project.getRootDirectory()), index)) {
            outputs.put(output.path(), output);
        }
        for (Path path : paths) {
            path = path.toAbsolutePath().normalize();
            outputs.put(path, new Output(path, path.getFileName().toString(), false, false, false));
        }
        Files.createDirectories(index.getParent());
        Path staged = Files.createTempFile(index.getParent(), ".reports-", ".tmp");
        try {
            Files.write(staged, serializeOutputs(project, List.copyOf(outputs.values())));
            try {
                Files.move(staged, index, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } catch (AtomicMoveNotSupportedException e) {
                Files.move(staged, index, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(staged);
        }
    }

    /**
     * Collects project-relative artifact paths from persistent build, bundle, and report indexes, even if files are absent.
     * Capture during resource synchronization, then use the captured paths as graph inputs.
     */
    public static Set<String> generatedPaths(Path projectRoot) throws IOException {
        projectRoot = projectRoot.toAbsolutePath().normalize();
        Path directory = directory(projectRoot);
        if (!Files.isDirectory(directory)) {
            return Set.of();
        }
        Set<String> paths = new LinkedHashSet<>();
        try (var indexes = Files.list(directory)) {
            List<Path> outputIndexes = indexes
                    .filter(path -> path.getFileName().toString().endsWith(".outputs"))
                    .sorted()
                    .toList();
            for (Path index : outputIndexes) {
                for (Output output : read(projectRoot, index)) {
                    if (output.path().startsWith(projectRoot)) {
                        paths.add(PackageResolver.normalize(projectRoot.relativize(output.path()).toString()));
                    }
                }
            }
        }
        return Set.copyOf(paths);
    }

    /**
     * Checks whether a normalized project path is an owned artifact, one of its descendants, or internal index data.
     */
    public static boolean isGeneratedPath(String path, Set<String> generatedPaths) {
        String normalized = PackageResolver.normalize(path);
        if (normalized.equals("/.internal/liveupdate") || normalized.startsWith("/.internal/liveupdate/")) {
            return true;
        }
        for (String generated : generatedPaths) {
            if (normalized.equals(generated) || normalized.startsWith(generated + "/")) {
                return true;
            }
        }
        return false;
    }

    /** Stops package work with InterruptedIOException on explicit cancellation or thread interruption. */
    static void checkCanceled(BooleanSupplier canceled) throws IOException {
        if (canceled.getAsBoolean() || Thread.currentThread().isInterrupted()) {
            throw new InterruptedIOException("Live Update package build cancelled");
        }
    }
}
