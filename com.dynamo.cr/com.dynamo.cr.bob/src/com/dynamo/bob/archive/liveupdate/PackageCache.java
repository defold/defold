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
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.function.BooleanSupplier;

import org.apache.commons.io.FileUtils;

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

/** Package fingerprints and artifact checks, independent of filesystem timestamps. */
final class PackageCache {

    static final String VERSION = "liveupdate-packages-v6";
    private static final ObjectMapper JSON = createMapper();

    /** Creates deterministic fingerprint serialization with sorted map keys and protobuf messages encoded as bytes. */
    private static ObjectMapper createMapper() {
        SimpleModule module = new SimpleModule();
        module.addSerializer(MessageLite.class, new JsonSerializer<MessageLite>() {
            @Override
            public void serialize(MessageLite value, JsonGenerator generator, SerializerProvider serializers) throws IOException {
                generator.writeBinary(value.toByteArray());
            }
        });
        return new ObjectMapper()
                .registerModule(module)
                .setVisibility(PropertyAccessor.ALL, JsonAutoDetect.Visibility.NONE)
                .setVisibility(PropertyAccessor.FIELD, JsonAutoDetect.Visibility.ANY)
                .enable(SerializationFeature.ORDER_MAP_ENTRIES_BY_KEYS);
    }

    /** Creates the SHA-256 digest used for package signatures and artifact validation. */
    private static MessageDigest digest() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new AssertionError(e);
        }
    }

    /** Hashes a length-prefixed value so adjacent fingerprint fields cannot merge ambiguously. */
    private static void add(MessageDigest digest, byte[] bytes) {
        digest.update(ByteBuffer.allocate(Long.BYTES).putLong(bytes.length).array());
        digest.update(bytes);
    }

    /** Hashes a length-prefixed UTF-8 field. */
    private static void add(MessageDigest digest, String text) {
        add(digest, text.getBytes(StandardCharsets.UTF_8));
    }

    /**
     * Fingerprints the cache format, engine version, provider identity, cache key, and available implementation
     * bytecode.
     */
    static String providerSignature(PackageProvider provider) throws IOException {
        MessageDigest digest = digest();
        add(digest, VERSION);
        add(digest, EngineVersion.sha1);
        add(digest, provider.getClass().getName());
        add(digest, JSON.writeValueAsBytes(provider.getCacheKey()));
        String classPath = "/" + provider.getClass().getName().replace('.', '/') + ".class";
        try (InputStream code = provider.getClass().getResourceAsStream(classPath)) {
            if (code != null) {
                add(digest, code.readAllBytes());
            }
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    /** Fingerprints effective Bob options independently of their map iteration order. */
    static String optionsSignature(Map<String, String> options) throws IOException {
        return HexFormat.of().formatHex(digest().digest(JSON.writeValueAsBytes(options)));
    }

    /**
     * Fingerprints provider inputs and the package manifest, whose resource hashes identify the prepared payloads.
     * Built-ins include only settings affecting their output; plugins include the complete context except cancellation.
     * Returns an empty string when the provider has not opted into artifact reuse.
     */
    static String inputSignature(PackageProvider provider, PackageProvider.Context context, byte[] manifest) throws IOException {
        if (provider.getCacheKey() == null) {
            return "";
        }
        MessageDigest digest = digest();
        add(digest, providerSignature(provider));
        add(digest, manifest);
        if (provider == PackageProviders.Builtin.ZIP) {
            add(digest, JSON.writeValueAsBytes(context.providerSettings().get("zip")));
        } else if (provider != PackageProviders.Builtin.FOLDER) {
            // Plans keep resource sets sorted; the mapper orders map keys. Omit the cancellation callback.
            List<Object> inputs = List.of(context.plan(), context.resources(), context.platform(),
                    context.architectures(), context.variant(), context.options(), context.projectSettings(),
                    context.providerSettings());
            add(digest, JSON.writeValueAsBytes(inputs));
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    /** Stops package work with InterruptedIOException on explicit cancellation or thread interruption. */
    static void checkCanceled(BooleanSupplier canceled) throws IOException {
        if (canceled.getAsBoolean() || Thread.currentThread().isInterrupted()) {
            throw new InterruptedIOException("Live Update package build cancelled");
        }
    }

    /**
     * Hashes contents, relative names, file types, and executable flags, ignoring timestamps.
     * Returns null for a missing artifact, wrong type, symlink, or unsupported filesystem entry.
     */
    static String contentHash(Path path, boolean directory, BooleanSupplier canceled) throws IOException {
        if (Files.isSymbolicLink(path) || !(directory ? Files.isDirectory(path) : Files.isRegularFile(path))) {
            return null;
        }
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
            if (Files.isSymbolicLink(file)) {
                return null;
            }
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

    /** Deletes an artifact tree or file if present, without following a root symlink as a directory. */
    static void delete(Path path) throws IOException {
        if (Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) {
            FileUtils.deleteDirectory(path.toFile());
        } else {
            Files.deleteIfExists(path);
        }
    }

    /**
     * Copies into an empty staging path, retaining attributes of unchanged previously delivered files.
     * The optional previous path is consulted for matching contents and executable flags, but is never modified.
     */
    static void copy(Path source, Path destination, Path previous, BooleanSupplier canceled) throws IOException {
        checkCanceled(canceled);
        if (Files.isDirectory(source)) {
            Files.createDirectories(destination);
            try (var entries = Files.list(source)) {
                for (Path entry : entries.toList()) {
                    Path name = entry.getFileName();
                    Path previousEntry = previous == null ? null : previous.resolve(name);
                    copy(entry, destination.resolve(name), previousEntry, canceled);
                }
            }
        } else {
            if (previous != null
                    && Files.isRegularFile(previous, LinkOption.NOFOLLOW_LINKS)
                    && Files.isExecutable(source) == Files.isExecutable(previous)
                    && Files.mismatch(source, previous) == -1) {
                source = previous;
            }
            Files.createDirectories(destination.getParent());
            Files.copy(source, destination, StandardCopyOption.COPY_ATTRIBUTES);
        }
    }
}
