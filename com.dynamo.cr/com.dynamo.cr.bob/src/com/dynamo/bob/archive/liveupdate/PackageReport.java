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
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.IResource;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Cached package contents for reports, independent of whether compiler tasks ran this time. */
public final class PackageReport {
    private static final ObjectMapper JSON = new ObjectMapper();

    public static IResource resource(Project project) {
        return project.getGameProjectResource().disableMinifyPath().changeExt(".lureport.json");
    }

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
            Path path = Path.of(project.getGameProjectResource().disableMinifyPath().changeExt(extension).getAbsPath());
            addOutput(base.withArray("outputs"), root, path, path.getFileName().toString(), false);
        }
        String certificates = project.getProjectProperties().getStringValue("network", "ssl_certificates");
        if (certificates != null && !certificates.isEmpty()) {
            addOutput(base.withArray("outputs"), root, root.resolve(project.getBuildDirectory()).resolve(BundleHelper.SSL_CERTIFICATES_NAME),
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
        resources.entrySet().stream().sorted(Map.Entry.comparingByKey()).forEach(entry -> rows.add(entry.getValue()));
        return JSON.writeValueAsBytes(report);
    }

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

    private static void addOutput(ArrayNode outputs, Path root, Path path, String name, boolean directory) {
        path = path.toAbsolutePath().normalize();
        ObjectNode output = outputs.addObject();
        output.put("name", name);
        output.put("path", (path.startsWith(root) ? root.relativize(path) : path).toString().replace('\\', '/'));
        output.put("directory", directory);
    }

    /** Measure finalized artifacts when generating a report, including after an incremental or bundle-only invocation. */
    public static ObjectNode read(Project project) throws IOException {
        IResource cache = resource(project);
        if (!cache.exists()) throw new IOException("Live Update report data is missing; run build before requesting a report");
        ObjectNode report = (ObjectNode) JSON.readTree(cache.getContent());
        Path root = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        for (var pkg : report.withArray("packages")) {
            long total = 0;
            boolean available = pkg.withArray("outputs").size() != 0;
            for (var output : pkg.withArray("outputs")) {
                Path path = root.resolve(output.get("path").asText());
                ((ObjectNode) output).put("path", path.toString().replace('\\', '/'));
                if (!Files.exists(path)) {
                    ((ObjectNode) output).putNull("size");
                    available = false;
                } else {
                    long size;
                    if (output.get("directory").asBoolean()) {
                        size = 0;
                        try (var files = Files.walk(path)) {
                            for (Path file : files.filter(Files::isRegularFile).toList()) size += Files.size(file);
                        }
                    } else size = Files.size(path);
                    ((ObjectNode) output).put("size", size);
                    total += size;
                }
            }
            if (available) ((ObjectNode) pkg).put("output_size", total);
            else ((ObjectNode) pkg).putNull("output_size");
        }
        return report;
    }
}
