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
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BooleanSupplier;

/** Implement in com.defold.extension.pipeline to add a package output format. */
public interface PackageProvider {
    record Artifact(String name, boolean directory) {}
    record ResourceInfo(String path, String hash, long size, long compressedSize, int flags) {}

    enum PropertyType { STRING, BOOLEAN, INTEGER, NUMBER }

    /** Values are authored as text; Bob and the editor share parsing and choice validation. */
    record Property(String id, String label, PropertyType type, String defaultValue, List<String> choices) {
        public Property {
            choices = List.copyOf(choices);
        }

        public Object parse(String value) {
            Object parsed;
            try {
                parsed = switch (type) {
                    case STRING -> value;
                    case BOOLEAN -> {
                        if (!value.equals("true") && !value.equals("false")) throw new IllegalArgumentException();
                        yield Boolean.valueOf(value);
                    }
                    case INTEGER -> Long.valueOf(value);
                    case NUMBER -> {
                        double number = Double.parseDouble(value);
                        if (!Double.isFinite(number)) throw new IllegalArgumentException();
                        yield number;
                    }
                };
            } catch (IllegalArgumentException e) {
                throw new IllegalArgumentException("Property '" + id + "' requires " + type.toString().toLowerCase(java.util.Locale.ROOT) + ", got '" + value + "'");
            }
            if (!choices.isEmpty() && !choices.contains(value)) {
                throw new IllegalArgumentException("Property '" + id + "' must be one of " + String.join(", ", choices) + ", got '" + value + "'");
            }
            return parsed;
        }
    }

    /** Opens the existing engine payload, including its 16-byte Live Update header. */
    final class PreparedResource {
        private final ResourceInfo info;
        private final Path payload;
        PreparedResource(ResourceInfo info, Path payload) {
            this.info = info;
            this.payload = payload;
        }
        public ResourceInfo info() { return info; }
        public InputStream openStream() throws IOException { return Files.newInputStream(payload); }
    }

    record Context(PackageResolver.Plan plan, Map<String, ResourceInfo> resources,
                   String platform, List<String> architectures, String variant,
                   Map<String, String> options, String projectSettings, Map<String, Map<String, Object>> providerSettings,
                   BooleanSupplier canceled) {
        public Context {
            resources = Map.copyOf(resources);
            architectures = List.copyOf(architectures);
            // CLI options without an argument can have a null value. Preserve their presence.
            options = Collections.unmodifiableMap(new LinkedHashMap<>(options));
            Map<String, Map<String, Object>> settings = new LinkedHashMap<>();
            providerSettings.forEach((id, values) -> settings.put(id, Collections.unmodifiableMap(new LinkedHashMap<>(values))));
            providerSettings = Collections.unmodifiableMap(settings);
        }
        public void checkCanceled() throws IOException {
            if (canceled.getAsBoolean() || Thread.currentThread().isInterrupted()) {
                throw new java.io.InterruptedIOException("Live Update package build cancelled");
            }
        }
    }

    String getId();

    default String getDisplayName() { return getId(); }

    /** Ordered properties shared by all packages using this provider. Defaults must parse successfully. */
    default List<Property> getProperties() { return List.of(); }

    /**
     * Opt in to reusing unchanged artifacts. Change this key when the provider implementation or
     * external inputs change. Bob also fingerprints the supplied context, manifest, and payloads.
     * A null key disables artifact reuse when package resolution runs.
     */
    default String getCacheKey() { return null; }

    List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) throws IOException, CompileExceptionError;
    void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
               byte[] manifest, Path stagingDirectory) throws IOException, CompileExceptionError;
}
