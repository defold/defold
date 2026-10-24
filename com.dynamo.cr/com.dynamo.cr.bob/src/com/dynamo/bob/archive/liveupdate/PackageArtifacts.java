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

import com.dynamo.bob.Project;
import com.dynamo.bob.util.MurmurHash;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Ownership outlives disposable build outputs, including clean and disabled export builds. */
public final class PackageArtifacts {
    private static Path directory(Path projectRoot) {
        return projectRoot.toAbsolutePath().normalize().resolve(".internal/liveupdate");
    }

    static Path buildReceipt(Project project) {
        String identity = project.getBuildDirectory() + "/" + project.getGameProjectResource().getPath();
        return directory(Path.of(project.getRootDirectory())).resolve("build-" + Long.toUnsignedString(MurmurHash.hash64(identity), 16) + ".outputs");
    }

    static Path bundleReceipt(Project project, Path bundleDirectory) {
        Path root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        Path destination = bundleDirectory.toAbsolutePath().normalize();
        String identity = destination.startsWith(root) ? root.relativize(destination).toString() : destination.toString();
        return directory(Path.of(project.getRootDirectory())).resolve("bundle-" +
                Long.toUnsignedString(MurmurHash.hash64(identity), 16) + ".outputs");
    }

    static List<PackageWriter.Output> read(Path projectRoot, Path receipt) throws IOException {
        return Files.exists(receipt) ? parse(projectRoot, Files.readString(receipt, StandardCharsets.UTF_8)) : List.of();
    }

    static String header(Project project) {
        return "project-root " + Base64.getEncoder().encodeToString(Path.of(project.getRootDirectory())
                .toAbsolutePath().normalize().toString().getBytes(StandardCharsets.UTF_8)) + "\n";
    }

    static List<PackageWriter.Output> parse(Path projectRoot, String content) throws IOException {
        projectRoot = projectRoot.toAbsolutePath().normalize();
        Path origin = null;
        if (content.startsWith("project-root ")) {
            int end = content.indexOf('\n');
            origin = Path.of(new String(Base64.getDecoder().decode(content.substring(13, end)), StandardCharsets.UTF_8));
            content = content.substring(end + 1);
        }
        List<PackageWriter.Output> outputs = new ArrayList<>();
        for (PackageWriter.Output output : PackageWriter.parseOutputs(content)) {
            Path path = output.path();
            if (origin != null && path.startsWith(origin)) {
                // A copied or moved project owns its own copies, never the original project's artifacts.
                path = projectRoot.resolve(origin.relativize(path));
            } else if (origin == null ? !path.startsWith(projectRoot) : !projectRoot.equals(origin)) {
                // External paths are owned only by the project that wrote the receipt.
                // Legacy receipts without an origin cannot establish external ownership.
                continue;
            }
            outputs.add(new PackageWriter.Output(path, output.name(), output.directory(), output.bundle(), output.inputSignature(), output.contentHash()));
        }
        return List.copyOf(outputs);
    }

    static List<PackageWriter.Output> readBuildOutputs(Project project) throws IOException {
        Path receipt = buildReceipt(project);
        return Files.exists(receipt) ? read(Path.of(project.getRootDirectory()), receipt) : PackageWriter.readOutputs(project);
    }

    /** Capture during resource synchronization, then use the captured paths as graph inputs. */
    public static Set<String> generatedPaths(Path projectRoot) throws IOException {
        projectRoot = projectRoot.toAbsolutePath().normalize();
        Path directory = directory(projectRoot);
        if (!Files.isDirectory(directory)) return Set.of();
        Set<String> paths = new LinkedHashSet<>();
        try (var receipts = Files.list(directory)) {
            for (Path receipt : receipts.filter(path -> path.getFileName().toString().endsWith(".outputs")).sorted().toList()) {
                for (PackageWriter.Output output : read(projectRoot, receipt)) {
                    if (output.path().startsWith(projectRoot)) {
                        paths.add(PackageResolver.normalize(projectRoot.relativize(output.path()).toString()));
                    }
                }
            }
        }
        return Set.copyOf(paths);
    }

    public static boolean isGeneratedPath(String path, Set<String> generatedPaths) {
        String normalized = PackageResolver.normalize(path);
        if (normalized.equals("/.internal/liveupdate") || normalized.startsWith("/.internal/liveupdate/")) return true;
        for (String generated : generatedPaths) {
            if (normalized.equals(generated) || normalized.startsWith(generated + "/")) return true;
        }
        return false;
    }
}
