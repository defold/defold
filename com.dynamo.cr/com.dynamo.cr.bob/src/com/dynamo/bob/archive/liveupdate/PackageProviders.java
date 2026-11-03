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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Shared provider discovery, property descriptions and settings resolution. */
public final class PackageProviders {
    /** The built-in containers share payload names, deduplication, and package-local manifests. */
    enum Builtin implements PackageProvider {
        ZIP, FOLDER;

        @Override
        public String getId() { return name().toLowerCase(Locale.ROOT); }

        @Override
        public String getDisplayName() { return this == ZIP ? "ZIP" : "Folder"; }

        @Override
        public List<Property> getProperties() {
            return this == ZIP ? List.of(new Property("compression-level", "Compression Level", PropertyType.INTEGER, "1",
                    List.of("-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"))) : List.of();
        }

        @Override
        public String getCacheKey() { return getId() + "-v1"; }

        @Override
        public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
            return List.of(new Artifact(pkg.description().getName() + (this == ZIP ? ".zip" : ""), this == FOLDER));
        }

        @Override
        public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
                          byte[] manifest, Path stagingDirectory) throws IOException {
            Path output = stagingDirectory.resolve(describeOutputs(context, pkg).getFirst().name());
            if (this == FOLDER) Files.createDirectory(output);
            try (ZipOutputStream zip = this == ZIP ? new ZipOutputStream(new BufferedOutputStream(Files.newOutputStream(output))) : null) {
                if (zip != null) zip.setLevel(((Number) context.providerSettings().get(getId()).get("compression-level")).intValue());
                Set<String> written = new HashSet<>();
                for (PreparedResource resource : resources) {
                    context.checkCanceled();
                    if (!written.add(resource.info().hash())) continue;
                    try (InputStream input = resource.openStream()) {
                        writeEntry(output, zip, resource.info().hash(), input);
                    }
                }
                writeEntry(output, zip, "liveupdate.game.dmanifest", new ByteArrayInputStream(manifest));
            }
        }

        private static void writeEntry(Path output, ZipOutputStream zip, String name, InputStream input) throws IOException {
            if (zip == null) {
                Files.copy(input, output.resolve(name));
            } else {
                ZipEntry entry = new ZipEntry(name);
                entry.setTime(0);
                zip.putNextEntry(entry);
                input.transferTo(zip);
                zip.closeEntry();
            }
        }
    }

    public record Schema(String label, List<PackageProvider.Property> properties) {
        public Schema { properties = List.copyOf(properties); }
    }

    private PackageProviders() {}

    static PackageProvider.Context context(Project project, PackageResolver.Plan plan,
                                           Map<String, PackageProvider> providers) throws IOException, CompileExceptionError {
        Map<String, PackageProvider.ResourceInfo> resources = new LinkedHashMap<>();
        for (List<ArchiveEntry> entries : List.of(project.getArchiveBuilder().getIncludedEntries(), project.getArchiveBuilder().getExcludedEntries())) {
            for (ArchiveEntry entry : entries) {
                PackageProvider.ResourceInfo info = new PackageProvider.ResourceInfo(entry);
                resources.put(info.path(), info);
            }
        }
        String manifestPath = PackageBuilder.manifestPath(project);
        ManifestDesc manifest = manifestPath.isEmpty()
                ? ManifestDesc.newBuilder().addAllPackages(plan.packages().values().stream().map(PackageResolver.Package::description).toList()).build()
                : PackageBuilder.parse(project.getResource(manifestPath).getContent());
        String architectureOption = project.option("architectures", null);
        List<String> architectures = architectureOption == null ? List.of(project.getPlatform().getArchitectures().getDefaultArchitectures()) :
                Arrays.stream(architectureOption.split(",")).map(String::trim).filter(value -> !value.isEmpty()).toList();
        return new PackageProvider.Context(plan, resources, project.getPlatform().getPair(), architectures,
                project.option("variant", "release"), project.getOptions(), project.getProjectProperties().serialize(),
                settings(manifest, schemas(providers), project.getPublisher().getPublisherSettings().getCompressionLevelValue()), project::isBuildCanceled);
    }

    static Map<String, PackageProvider> discover(IClassScanner scanner) throws CompileExceptionError {
        Map<String, PackageProvider> providers = new LinkedHashMap<>();
        for (PackageProvider provider : Builtin.values()) providers.put(provider.getId(), provider);
        List<PackageProvider> plugins = PluginScanner.getOrCreatePlugins(scanner, "com.defold.extension.pipeline", PackageProvider.class);
        if (plugins != null) {
            for (PackageProvider provider : plugins) {
                String id = provider.getId();
                if (id == null || id.isBlank() || providers.putIfAbsent(id, provider) != null) {
                    throw new CompileExceptionError("Invalid or duplicate Live Update provider: " + id);
                }
            }
        }
        return Collections.unmodifiableMap(providers);
    }

    /** Used by the editor after loading pipeline plugins, independently of package compilation. */
    public static Map<String, Schema> describe(IClassScanner scanner) throws CompileExceptionError {
        return schemas(discover(scanner));
    }

    static Map<String, Schema> schemas(Map<String, PackageProvider> providers) throws CompileExceptionError {
        Map<String, Schema> schemas = new LinkedHashMap<>();
        for (var entry : providers.entrySet()) {
            PackageProvider provider = entry.getValue();
            List<PackageProvider.Property> properties = List.copyOf(provider.getProperties());
            Set<String> ids = new LinkedHashSet<>();
            for (PackageProvider.Property property : properties) {
                if (property.id().isBlank() || !ids.add(property.id())) {
                    throw new CompileExceptionError("Invalid or duplicate property '" + property.id() + "' in Live Update provider '" + entry.getKey() + "'");
                }
                try {
                    property.parse(property.defaultValue());
                    for (String choice : property.choices()) property.parse(choice);
                } catch (IllegalArgumentException e) {
                    throw new CompileExceptionError("Invalid property description in Live Update provider '" + entry.getKey() + "': " + e.getMessage(), e);
                }
            }
            schemas.put(entry.getKey(), new Schema(provider.getDisplayName(), properties));
        }
        return Collections.unmodifiableMap(schemas);
    }

    /** Retain legacy ZIP compression until the manifest explicitly overrides it. */
    public static String defaultValue(String provider, PackageProvider.Property property, String legacyZipCompression) {
        return legacyZipCompression != null && provider.equals("zip") && property.id().equals("compression-level")
                ? legacyZipCompression : property.defaultValue();
    }

    static Map<String, Map<String, Object>> settings(ManifestDesc manifest, Map<String, Schema> schemas,
                                                    String legacyZipCompression) throws CompileExceptionError {
        Set<String> used = new LinkedHashSet<>();
        manifest.getPackagesList().forEach(pkg -> used.add(pkg.getProvider()));
        Map<String, Map<String, String>> authored = new LinkedHashMap<>();
        for (var provider : manifest.getProvidersList()) {
            if (!used.contains(provider.getProvider())) continue;
            Map<String, String> values = new LinkedHashMap<>();
            if (authored.putIfAbsent(provider.getProvider(), values) != null) {
                throw new CompileExceptionError("Duplicate Live Update provider settings: " + provider.getProvider());
            }
            for (var property : provider.getPropertiesList()) {
                if (values.putIfAbsent(property.getName(), property.getValue()) != null) {
                    throw new CompileExceptionError("Duplicate property '" + property.getName() + "' in Live Update provider '" + provider.getProvider() + "'");
                }
            }
        }
        Map<String, Map<String, Object>> result = new LinkedHashMap<>();
        for (String id : used) {
            Schema schema = schemas.get(id);
            if (schema == null) throw new CompileExceptionError("Unknown Live Update provider: " + id);
            Map<String, String> values = new LinkedHashMap<>(authored.getOrDefault(id, Map.of()));
            Map<String, Object> parsed = new LinkedHashMap<>();
            for (PackageProvider.Property property : schema.properties()) {
                String value = values.remove(property.id());
                if (value == null) value = defaultValue(id, property, legacyZipCompression);
                try {
                    parsed.put(property.id(), property.parse(value));
                } catch (IllegalArgumentException e) {
                    throw new CompileExceptionError("Live Update provider '" + id + "': " + e.getMessage(), e);
                }
            }
            if (!values.isEmpty()) throw new CompileExceptionError("Unknown properties in Live Update provider '" + id + "': " + String.join(", ", values.keySet()));
            result.put(id, Map.copyOf(parsed));
        }
        return Map.copyOf(result);
    }
}
