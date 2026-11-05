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
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.BooleanSupplier;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;

/** Implement in com.defold.extension.pipeline to add a package output format. */
public interface PackageProvider {

    /**
     * An output path relative to the output root and provider staging directory; directories include their contents.
     */
    record Artifact(String name, boolean directory) {}

    /** Compiled payload metadata. Sizes exclude the Live Update header; compressedSize is -1 when uncompressed. */
    record ResourceInfo(String path, String hash, long size, long compressedSize, int flags) {

        /** Captures archive metadata with the Live Update flag set for external delivery. */
        ResourceInfo(ArchiveEntry entry) {
            this(PackageResolver.normalize(entry.getRelativeFilename()), entry.getHexDigest(), entry.getSize(),
                    entry.getCompressedSize(), entry.getFlags() | ArchiveEntry.FLAG_LIVEUPDATE);
        }
    }

    enum PropertyType {
        STRING,
        BOOLEAN,
        INTEGER,
        NUMBER
    }

    /** Values are authored as text; Bob and the editor share parsing and choice validation. */
    record Property(String id, String label, PropertyType type, String defaultValue, List<String> choices) {
        public Property {
            choices = List.copyOf(choices);
        }

        /**
         * Parses an authored value and checks any allowed choices against its original text.
         * Returns String, Boolean, Long, or Double according to the property type.
         *
         * @throws IllegalArgumentException if the type, finite-number check, or choice validation fails
         */
        public Object parse(String value) {
            Object parsed;
            try {
                parsed = switch (type) {
                    case STRING -> value;
                    case BOOLEAN -> {
                        if (!value.equals("true") && !value.equals("false")) {
                            throw new IllegalArgumentException();
                        }
                        yield Boolean.valueOf(value);
                    }
                    case INTEGER -> Long.valueOf(value);
                    case NUMBER -> {
                        double number = Double.parseDouble(value);
                        if (!Double.isFinite(number)) {
                            throw new IllegalArgumentException();
                        }
                        yield number;
                    }
                };
            } catch (IllegalArgumentException e) {
                String typeName = type.toString().toLowerCase(Locale.ROOT);
                throw new IllegalArgumentException(
                        "Property '" + id + "' requires " + typeName + ", got '" + value + "'");
            }
            if (!choices.isEmpty() && !choices.contains(value)) {
                throw new IllegalArgumentException("Property '" + id + "' must be one of " + String.join(", ", choices)
                        + ", got '" + value + "'");
            }
            return parsed;
        }
    }

    /** Opens the existing engine payload, including its 16-byte Live Update header. */
    final class PreparedResource {

        private final ResourceInfo info;
        private final Path payload;

        /** References a prepared payload owned by the writer; this wrapper does not copy or delete it. */
        PreparedResource(ResourceInfo info, Path payload) {
            this.info = info;
            this.payload = payload;
        }

        /** Returns the resource path, content hash, sizes, and archive flags. */
        public ResourceInfo info() {
            return info;
        }

        /** Opens a fresh stream including the 16-byte Live Update header. The caller must close each stream. */
        public InputStream openStream() throws IOException {
            return Files.newInputStream(payload);
        }
    }

    /**
     * Read-only build data shared by provider callbacks, including the full plan and all prepared resource metadata.
     * Provider settings are parsed values indexed by provider and property id; projectSettings is serialized game.project data.
     * The cancellation callback remains live for the duration of writing.
     */
    record Context(PackageResolver.Plan plan,
            Map<String, ResourceInfo> resources,
            String platform,
            List<String> architectures,
            String variant,
            Map<String, String> options,
            String projectSettings,
            Map<String, Map<String, Object>> providerSettings,
            BooleanSupplier canceled) {
        public Context {
            resources = Collections.unmodifiableMap(new LinkedHashMap<>(resources));
            architectures = List.copyOf(architectures);
            // CLI options without an argument can have a null value. Preserve their presence.
            options = Collections.unmodifiableMap(new LinkedHashMap<>(options));
            Map<String, Map<String, Object>> settings = new LinkedHashMap<>();
            for (Map.Entry<String, Map<String, Object>> entry : providerSettings.entrySet()) {
                Map<String, Object> values = Collections.unmodifiableMap(new LinkedHashMap<>(entry.getValue()));
                settings.put(entry.getKey(), values);
            }
            providerSettings = Collections.unmodifiableMap(settings);
        }

        /** Throws InterruptedIOException when the build is cancelled or the current thread is interrupted. */
        public void checkCanceled() throws IOException {
            PackageArtifacts.checkCanceled(canceled);
        }
    }

    /** Returns the stable, nonblank manifest identifier. It must be unique among built-in and plugin providers. */
    String getId();

    /** Returns the provider label used by authoring tools; defaults to the manifest identifier. */
    default String getDisplayName() {
        return getId();
    }

    /** Ordered properties shared by all packages using this provider. Defaults must parse successfully. */
    default List<Property> getProperties() {
        return List.of();
    }

    /**
     * Declares every output before any provider writes, allowing Bob to validate destination collisions.
     * Names must be normalized relative paths without parent traversal or backslashes. This operation must not write files.
     * The context and package are the same resolved inputs used by {@link #write}.
     */
    List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) throws IOException, CompileExceptionError;

    /**
     * Writes the declared artifacts beneath the assigned staging directory without changing package membership.
     * Bob finalizes the outputs only after all providers succeed; throw on failure and check cancellation during long work.
     *
     * @param resources repeatable streams of prepared engine payloads for this package; close streams after reading
     * @param manifest liveupdate.game.dmanifest bytes containing exactly this package's resource entries
     * @param stagingDirectory empty directory owned and cleaned up by Bob; do not retain streams or write outside it
     */
    void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
            byte[] manifest, Path stagingDirectory) throws IOException, CompileExceptionError;
}
