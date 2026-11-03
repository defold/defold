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
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.IResource;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

/** Cached package contents for reports, independent of whether compiler tasks ran this time. */
public final class PackageReport {

    private static final ObjectMapper JSON = new ObjectMapper();

    /**
     * Returns the .lureport.json build output used to retain package contents across incremental and bundle-only
     * invocations.
     */
    public static IResource resource(Project project) {
        return project.getGameProjectResource().disableMinifyPath().changeExt(".lureport.json");
    }

    /**
     * Serializes package membership, source attribution, compiled sizes, and declared outputs as cached JSON.
     * Container sizes are measured later by {@link #read}; this method does not write the report resource.
     */
    public static byte[] create(Project project, ArchiveBuilder archive, PackageResolver.Plan plan,
            Map<String, List<PackageArtifacts.Output>> outputs) throws IOException {
        ObjectNode report = JSON.createObjectNode();
        Map<String, ObjectNode> resources = new LinkedHashMap<>();
        for (List<ArchiveEntry> entries : List.of(archive.getIncludedEntries(), archive.getExcludedEntries())) {
            for (ArchiveEntry entry : entries) {
                String path = PackageResolver.normalize(entry.getRelativeFilename());
                ObjectNode row = JSON.createObjectNode();
                row.put("file", path);
                row.put("source", plan.primarySources().getOrDefault(path, ""));
                row.put("size", entry.getSize());
                row.put("compressed_size", entry.isCompressed() ? entry.getCompressedSize() : entry.getSize());
                row.put("encrypted", entry.isEncrypted());
                row.putArray("packages");
                resources.put(path, row);
            }
        }

        ArrayNode packages = report.putArray("packages");
        ObjectNode base = addPackage(packages, resources, "base", "archive", plan.base());
        Path root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        for (String extension : List.of(".projectc", ".arci", ".arcd", ".dmanifest")) {
            IResource output = project.getGameProjectResource().disableMinifyPath().changeExt(extension);
            Path path = Path.of(output.getAbsPath());
            addOutput(base.withArray("outputs"), root, path, path.getFileName().toString(), false);
        }

        String certificates = project.getProjectProperties().getStringValue("network", "ssl_certificates");
        if (certificates != null && !certificates.isEmpty()) {
            Path path = root.resolve(project.getBuildDirectory()).resolve(BundleHelper.SSL_CERTIFICATES_NAME);
            addOutput(base.withArray("outputs"), root, path,
                    BundleHelper.SSL_CERTIFICATES_NAME, false);
        }

        for (PackageResolver.Package pkg : plan.packages().values()) {
            String name = pkg.description().getName();
            ObjectNode row = addPackage(packages, resources, name, pkg.description().getProvider(), pkg.resources());
            for (PackageArtifacts.Output output : outputs.getOrDefault(name, List.of())) {
                addOutput(row.withArray("outputs"), root, output.path(), output.name(), output.directory());
            }
        }

        ArrayNode rows = report.putArray("package_resources");
        for (String path : resources.keySet().stream().sorted().toList()) {
            rows.add(resources.get(path));
        }
        return JSON.writeValueAsBytes(report);
    }

    /** Adds a package row, records each resource membership, and sums uncompressed compiled bytes. */
    private static ObjectNode addPackage(ArrayNode packages, Map<String, ObjectNode> resources,
            String name, String provider, Collection<String> contents) {
        ObjectNode pkg = packages.addObject();
        pkg.put("name", name);
        pkg.put("provider", provider);
        pkg.putArray("outputs");
        ArrayNode paths = pkg.putArray("resources");
        long size = 0;
        for (String path : contents.stream().sorted().toList()) {
            ObjectNode resource = resources.get(path);
            resource.withArray("packages").add(name);
            paths.add(path);
            size += resource.get("size").asLong();
        }
        pkg.put("size", size);
        return pkg;
    }

    /** Adds an artifact reference, storing a project-relative path when the destination is inside the project. */
    private static void addOutput(ArrayNode outputs, Path root, Path path, String name, boolean directory) {
        path = path.toAbsolutePath().normalize();
        Path reportPath = path.startsWith(root) ? root.relativize(path) : path;
        ObjectNode output = outputs.addObject();
        output.put("name", name);
        output.put("path", reportPath.toString().replace('\\', '/'));
        output.put("directory", directory);
    }

    /**
     * Reads cached package contents and measures finalized artifacts for an incremental or bundle-only report.
     * Missing artifacts have null sizes; missing cached report data requires a build and raises IOException.
     */
    public static ObjectNode read(Project project) throws IOException {
        IResource cache = resource(project);
        if (!cache.exists()) {
            throw new IOException("Live Update report data is missing; run build before requesting a report");
        }

        ObjectNode report = (ObjectNode) JSON.readTree(cache.getContent());
        Path root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        for (JsonNode packageNode : report.withArray("packages")) {
            ObjectNode pkg = (ObjectNode) packageNode;
            long total = 0;
            boolean available = pkg.withArray("outputs").size() != 0;
            for (JsonNode outputNode : pkg.withArray("outputs")) {
                ObjectNode output = (ObjectNode) outputNode;
                Path path = root.resolve(output.get("path").asText());
                output.put("path", path.toString().replace('\\', '/'));
                if (!Files.exists(path)) {
                    output.putNull("size");
                    available = false;
                } else {
                    long size;
                    if (output.get("directory").asBoolean()) {
                        size = 0;
                        try (var files = Files.walk(path)) {
                            for (Path file : files.filter(Files::isRegularFile).toList()) {
                                size += Files.size(file);
                            }
                        }
                    } else {
                        size = Files.size(path);
                    }
                    output.put("size", size);
                    total += size;
                }
            }

            if (available) {
                pkg.put("output_size", total);
            } else {
                pkg.putNull("output_size");
            }
        }
        return report;
    }
}
