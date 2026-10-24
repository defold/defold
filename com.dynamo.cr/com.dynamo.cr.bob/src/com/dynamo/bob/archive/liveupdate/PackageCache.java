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

import com.dynamo.bob.archive.EngineVersion;
import com.fasterxml.jackson.annotation.JsonAutoDetect;
import com.fasterxml.jackson.annotation.PropertyAccessor;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.databind.JsonSerializer;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.databind.SerializerProvider;
import com.fasterxml.jackson.databind.module.SimpleModule;
import com.google.protobuf.MessageLite;
import org.apache.commons.io.FileUtils;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.function.BooleanSupplier;

/** Package fingerprints and artifact checks, independent of filesystem timestamps. */
final class PackageCache {
    static final String VERSION = "liveupdate-packages-v5";
    private static final ObjectMapper JSON = createMapper();

    private static ObjectMapper createMapper() {
        SimpleModule module = new SimpleModule();
        module.addSerializer(MessageLite.class, new JsonSerializer<MessageLite>() {
            @Override
            public void serialize(MessageLite value, JsonGenerator generator, SerializerProvider serializers) throws IOException {
                generator.writeBinary(value.toByteArray());
            }
        });
        return new ObjectMapper().registerModule(module)
                .setVisibility(PropertyAccessor.ALL, JsonAutoDetect.Visibility.NONE)
                .setVisibility(PropertyAccessor.FIELD, JsonAutoDetect.Visibility.ANY)
                .enable(SerializationFeature.ORDER_MAP_ENTRIES_BY_KEYS);
    }

    private static MessageDigest digest() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new AssertionError(e);
        }
    }

    private static void add(MessageDigest digest, byte[] bytes) {
        digest.update(ByteBuffer.allocate(Long.BYTES).putLong(bytes.length).array());
        digest.update(bytes);
    }

    private static void add(MessageDigest digest, String text) {
        add(digest, text.getBytes(StandardCharsets.UTF_8));
    }

    static String providerSignature(PackageProvider provider) throws IOException {
        MessageDigest digest = digest();
        add(digest, VERSION);
        add(digest, EngineVersion.sha1);
        add(digest, provider.getClass().getName());
        add(digest, JSON.writeValueAsBytes(provider.getCacheKey()));
        try (InputStream code = provider.getClass().getResourceAsStream("/" + provider.getClass().getName().replace('.', '/') + ".class")) {
            if (code != null) add(digest, code.readAllBytes());
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    static String optionsSignature(Map<String, String> options) throws IOException {
        return HexFormat.of().formatHex(digest().digest(JSON.writeValueAsBytes(options)));
    }

    static String inputSignature(PackageProvider provider, PackageProvider.Context context, byte[] manifest) throws IOException {
        if (provider.getCacheKey() == null) return "";
        MessageDigest digest = digest();
        add(digest, providerSignature(provider));
        add(digest, manifest);
        if (provider instanceof ZipPackageProvider) {
            add(digest, JSON.writeValueAsBytes(context.providerSettings().get("zip")));
        } else if (!(provider instanceof FolderPackageProvider)) {
            // Custom providers can inspect the entire context, including other packages and
            // provenance. Canonicalize unordered collections before fingerprinting it.
            PackageResolver.Plan plan = context.plan();
            Map<String, PackageResolver.Package> packages = new LinkedHashMap<>();
            plan.packages().forEach((name, pkg) -> packages.put(name,
                    new PackageResolver.Package(pkg.description(), new TreeSet<>(pkg.resources()), pkg.provenance())));
            PackageResolver.Plan canonicalPlan = new PackageResolver.Plan(packages, new TreeSet<>(plan.base()),
                    plan.locks(), plan.sourceOutputs(), plan.primarySources(), plan.resources(), plan.diagnostics(), plan.requirements());
            add(digest, JSON.writeValueAsBytes(List.of(canonicalPlan, new TreeMap<>(context.resources()), context.platform(),
                    context.architectures(), context.variant(), context.options(), context.projectSettings(), context.providerSettings())));
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    private static void checkCanceled(BooleanSupplier canceled) throws IOException {
        if (canceled.getAsBoolean() || Thread.currentThread().isInterrupted()) {
            throw new java.io.InterruptedIOException("Live Update package build cancelled");
        }
    }

    /** Null means a missing artifact, wrong type, or symlink; all require regeneration. */
    static String contentHash(Path path, boolean directory, BooleanSupplier canceled) throws IOException {
        if (Files.isSymbolicLink(path) || !(directory ? Files.isDirectory(path) : Files.isRegularFile(path))) return null;
        MessageDigest digest = digest();
        List<Path> files;
        if (directory) {
            try (var entries = Files.walk(path)) {
                files = entries.sorted().toList();
            }
        } else {
            files = List.of(path);
        }
        byte[] buffer = new byte[64 * 1024];
        for (Path file : files) {
            checkCanceled(canceled);
            if (Files.isSymbolicLink(file)) return null;
            add(digest, path.relativize(file).toString().replace('\\', '/'));
            if (Files.isDirectory(file)) {
                add(digest, "directory");
            } else if (Files.isRegularFile(file)) {
                add(digest, "file");
                add(digest, Boolean.toString(Files.isExecutable(file)));
                add(digest, Long.toString(Files.size(file)));
                try (InputStream input = Files.newInputStream(file)) {
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        checkCanceled(canceled);
                        digest.update(buffer, 0, count);
                    }
                }
            } else {
                return null;
            }
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    static void delete(Path path) throws IOException {
        if (Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) FileUtils.deleteDirectory(path.toFile());
        else Files.deleteIfExists(path);
    }

    /** Update only changed files and remove stale files inside this owned artifact. */
    static void copy(Path source, Path destination, BooleanSupplier canceled) throws IOException {
        checkCanceled(canceled);
        if (Files.isDirectory(source)) {
            if (!Files.isDirectory(destination, LinkOption.NOFOLLOW_LINKS)) {
                delete(destination);
                Files.createDirectories(destination);
            }
            try (var entries = Files.list(destination)) {
                for (Path entry : entries.toList()) {
                    checkCanceled(canceled);
                    if (!Files.exists(source.resolve(entry.getFileName()), LinkOption.NOFOLLOW_LINKS)) delete(entry);
                }
            }
            try (var entries = Files.list(source)) {
                for (Path entry : entries.toList()) copy(entry, destination.resolve(entry.getFileName()), canceled);
            }
        } else if (!Files.isRegularFile(destination, LinkOption.NOFOLLOW_LINKS) ||
                   Files.isExecutable(source) != Files.isExecutable(destination) || Files.mismatch(source, destination) != -1) {
            delete(destination);
            Files.createDirectories(destination.getParent());
            Files.copy(source, destination, StandardCopyOption.COPY_ATTRIBUTES);
        }
    }
}
