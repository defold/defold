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
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.liveupdate.proto.Manifest.PackageMetadata;
import com.dynamo.liveupdate.proto.Manifest.PackageOutput;
import com.dynamo.liveupdate.proto.Manifest.PackageRequirement;
import org.apache.commons.io.FileUtils;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Collection;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/** Owns temporary payloads and the transaction that publishes a complete set of artifacts. */
public final class PackageWriter implements ArchiveBuilder.PreparedResourceConsumer, AutoCloseable {
    public record Output(Path path, String name, boolean directory, boolean bundle, String inputSignature, String contentHash) {}
    private record Delivery(PackageResolver.Package pkg, PackageProvider provider, Path stage,
                            List<PackageProvider.Artifact> artifacts, Path root, boolean bundle) {}

    private final Project project;
    private final PackageResolver.Plan plan;
    private final Path stage;
    private final Map<String, PackageProvider.PreparedResource> prepared = new LinkedHashMap<>();
    private final Map<String, PackageProvider.ResourceInfo> resourceInfo = new LinkedHashMap<>();
    private final Set<String> packaged = new TreeSet<>();

    public PackageWriter(Project project, PackageResolver.Plan plan) throws IOException {
        this.project = project;
        this.plan = plan;
        Path buildDirectory = Path.of(project.getRootDirectory(), project.getBuildDirectory());
        Files.createDirectories(buildDirectory);
        stage = Files.createTempDirectory(buildDirectory, ".liveupdate-");
        Files.createDirectory(stage.resolve("payloads"));
        for (PackageResolver.Package pkg : plan.packages().values()) packaged.addAll(pkg.resources());
    }

    @Override
    public synchronized void accept(ArchiveEntry entry, byte[] payload) throws IOException {
        checkCanceled();
        String path = PackageResolver.normalize(entry.getRelativeFilename());
        int flags = entry.getFlags() | ArchiveEntry.FLAG_LIVEUPDATE;
        PackageProvider.ResourceInfo info = new PackageProvider.ResourceInfo(path, entry.getHexDigest(), entry.getSize(), entry.getCompressedSize(), flags);
        resourceInfo.put(path, info);
        if (!packaged.contains(path)) return;
        Path file = stage.resolve("payloads").resolve(entry.getHexDigest());
        if (!Files.exists(file)) {
            byte[] padding = new byte[11];
            Arrays.fill(padding, (byte) 0xED);
            byte[] header = ByteBuffer.allocate(16).putInt(entry.getSize()).put((byte) flags).put(padding).array();
            try (OutputStream output = Files.newOutputStream(file)) {
                output.write(header);
                output.write(payload);
            }
        }
        prepared.put(path, new PackageProvider.PreparedResource(info, file));
    }

    private void checkCanceled() throws IOException {
        checkCanceled(project);
    }

    private static void checkCanceled(Project project) throws IOException {
        if (project.isBuildCanceled() || Thread.currentThread().isInterrupted()) throw new java.io.InterruptedIOException("Live Update package build cancelled");
    }

    public static Map<String, PackageProvider> providers(Project project) throws CompileExceptionError {
        return PackageProviders.discover(project.getProjectClassLoaderScanner());
    }

    static PackageProvider.Context createContext(Project project, PackageResolver.Plan plan,
                                                 Map<String, PackageProvider.ResourceInfo> resourceInfo) throws IOException, CompileExceptionError {
        PublisherSettings settings = project.getPublisher().getPublisherSettings();
        String architectureOption = project.option("architectures", null);
        List<String> architectures = architectureOption == null ? List.of(project.getPlatform().getArchitectures().getDefaultArchitectures()) :
                Arrays.stream(architectureOption.split(",")).map(String::trim).filter(value -> !value.isEmpty()).toList();
        return new PackageProvider.Context(plan, resourceInfo,
                project.getPlatform().getPair(), architectures,
                project.option("variant", "release"), project.getOptions(), project.getProjectProperties().serialize(),
                PackageProviders.settings(PackageBuilder.parse(project.getResource(PackageBuilder.manifestPath(project)).getContent()),
                        PackageProviders.schemas(providers(project)), settings.getCompressionLevelValue()), project::isBuildCanceled);
    }

    public List<Output> write(ManifestBuilder manifests) throws IOException, CompileExceptionError {
        PublisherSettings settings = project.getPublisher().getPublisherSettings();
        PackageProvider.Context context = createContext(project, plan, resourceInfo);
        Map<String, PackageProvider> providers = providers(project);
        Path root = PackageBuilder.outputDirectory(project, Path.of(project.getRootDirectory()));
        Map<Path, Output> previousOutputs = new LinkedHashMap<>();
        for (Output output : PackageArtifacts.readBuildOutputs(project)) previousOutputs.put(output.path(), output);
        List<Delivery> deliveries = new ArrayList<>();
        Map<Path, Path> stagedOutputs = new LinkedHashMap<>();
        List<Output> outputs = new ArrayList<>();
        List<Path> occupied = new ArrayList<>();
        List<PackageOutput> references = new ArrayList<>();
        for (PackageResolver.Package pkg : plan.packages().values()) {
            context.checkCanceled();
            String name = pkg.description().getName();
            String providerId = pkg.description().getProvider();
            PackageProvider provider = providers.get(providerId);
            if (provider == null) throw new CompileExceptionError("Unknown Live Update provider: " + providerId);
            List<PackageProvider.Artifact> artifacts = List.copyOf(provider.describeOutputs(context, pkg));
            boolean bundle = providerId.equals("zip") ? settings.getSaveZipInBundleFolder() : settings.getSaveFolderInBundleFolder();
            Path packageStage = Files.createDirectory(stage.resolve("package-" + deliveries.size()));
            PackageOutput.Builder reference = PackageOutput.newBuilder().setPackage(name).setProvider(providerId);
            for (PackageProvider.Artifact artifact : artifacts) {
                Path relative = Path.of(artifact.name());
                if (relative.isAbsolute() || relative.getNameCount() == 0 || !relative.normalize().equals(relative) ||
                        relative.startsWith("..") || artifact.name().isBlank() || artifact.name().contains("\\")) {
                    throw new CompileExceptionError("Invalid package artifact path: " + artifact.name());
                }
                Path destination = root.resolve(relative).normalize();
                Path resolvedDestination = resolveExistingPath(destination);
                Path collisionKey = Path.of(destination.toString().toLowerCase(Locale.ROOT));
                for (Path previous : occupied) {
                    if (collisionKey.startsWith(previous) || previous.startsWith(collisionKey)) {
                        throw new CompileExceptionError("Colliding Live Update outputs: " + destination);
                    }
                }
                occupied.add(collisionKey);
                for (String source : plan.sourceOutputs().keySet()) {
                    Path sourcePath = Path.of(project.getRootDirectory()).resolve(source.substring(1)).toAbsolutePath().normalize();
                    if (resolveExistingPath(sourcePath).startsWith(resolvedDestination)) {
                        throw new CompileExceptionError("Package output would overwrite a source resource: " + destination);
                    }
                }
                if (Files.exists(destination) && !previousOutputs.containsKey(destination)) {
                    throw new CompileExceptionError("Package output would overwrite an existing file or directory: " + destination);
                }
                reference.addArtifacts(artifact.name());
            }
            references.add(reference.build());
            deliveries.add(new Delivery(pkg, provider, packageStage, artifacts, root, bundle));
        }
        for (Delivery delivery : deliveries) {
            context.checkCanceled();
            PackageResolver.Package pkg = delivery.pkg();
            PackageMetadata.Builder metadata = PackageMetadata.newBuilder().setName(pkg.description().getName()).addAllOutputs(references);
            plan.requirements().get(pkg.description().getName()).forEach((path, alternatives) ->
                    metadata.addRequirements(PackageRequirement.newBuilder().setUrlHash(MurmurHash.hash64(path)).addAllPackages(alternatives)));
            List<PackageProvider.PreparedResource> resources = pkg.resources().stream().sorted().map(prepared::get).toList();
            if (resources.contains(null)) throw new IOException("Missing prepared resource in " + pkg.description().getName());
            byte[] manifest = manifests.buildPackageManifest(pkg.resources(), metadata.build());
            String signature = PackageCache.inputSignature(delivery.provider(), context, manifest);
            boolean reuse = !signature.isEmpty() && !delivery.artifacts().isEmpty();
            for (PackageProvider.Artifact artifact : delivery.artifacts()) {
                Output previous = previousOutputs.get(delivery.root().resolve(artifact.name()));
                if (previous == null || previous.directory() != artifact.directory() ||
                        !signature.equals(previous.inputSignature()) || !isCurrent(previous, project)) {
                    reuse = false;
                    break;
                }
            }
            if (reuse) {
                for (PackageProvider.Artifact artifact : delivery.artifacts()) {
                    Output previous = previousOutputs.get(delivery.root().resolve(artifact.name()));
                    outputs.add(new Output(previous.path(), artifact.name(), artifact.directory(), delivery.bundle(), signature, previous.contentHash()));
                }
                continue;
            }
            delivery.provider().write(context, pkg, resources, manifest, delivery.stage());
            for (PackageProvider.Artifact artifact : delivery.artifacts()) {
                Path output = delivery.stage().resolve(artifact.name());
                String hash = PackageCache.contentHash(output, artifact.directory(), project::isBuildCanceled);
                if (hash == null) throw new IOException("Provider did not produce a regular, non-symlink output: " + artifact.name());
                Path destination = delivery.root().resolve(artifact.name());
                stagedOutputs.put(destination, output);
                outputs.add(new Output(destination, artifact.name(), artifact.directory(), delivery.bundle(), signature, hash));
            }
        }
        context.checkCanceled();
        Set<Path> obsolete = new LinkedHashSet<>(previousOutputs.keySet());
        for (Output output : outputs) obsolete.remove(output.path());
        Path ownership = stage.resolve("ownership.outputs");
        Files.write(ownership, serializeOutputs(project, outputs));
        stagedOutputs.put(PackageArtifacts.buildReceipt(project), ownership);
        finalizeOutputs(project, stagedOutputs, obsolete);
        return List.copyOf(outputs);
    }

    /** Resolve existing ancestors as well, so aliases cannot hide a source beneath a new output directory. */
    private static Path resolveExistingPath(Path path) throws IOException {
        if (Files.exists(path)) return path.toRealPath();
        Path parent = path.getParent();
        return parent == null ? path : resolveExistingPath(parent).resolve(path.getFileName());
    }

    private static void finalizeOutputs(Project project, Map<Path, Path> outputs, Set<Path> obsolete) throws IOException {
        Set<Path> affected = new LinkedHashSet<>(obsolete);
        affected.addAll(outputs.keySet());
        List<Path> roots = new ArrayList<>();
        for (Path path : affected.stream().sorted(Comparator.comparingInt(Path::getNameCount)).toList()) {
            if (roots.stream().noneMatch(path::startsWith)) roots.add(path);
        }
        Map<Path, Path> backups = new LinkedHashMap<>();
        boolean installing = false;
        boolean committed = false;
        try {
            for (Path destination : roots) {
                checkCanceled(project);
                if (Files.exists(destination)) {
                    Path backup = Files.createTempDirectory(destination.getParent(), ".liveupdate-backup-").resolve("previous");
                    Files.move(destination, backup);
                    backups.put(destination, backup);
                }
            }
            installing = true;
            for (Map.Entry<Path, Path> output : outputs.entrySet()) {
                checkCanceled(project);
                Path destination = output.getKey();
                Files.createDirectories(destination.getParent());
                if (Files.isDirectory(output.getValue())) FileUtils.moveDirectory(output.getValue().toFile(), destination.toFile());
                else Files.move(output.getValue(), destination, StandardCopyOption.REPLACE_EXISTING);
            }
            checkCanceled(project);
            committed = true;
        } catch (IOException e) {
            if (installing) {
                for (Path path : roots) {
                    try { PackageCache.delete(path); }
                    catch (IOException cleanupError) { e.addSuppressed(cleanupError); }
                }
            }
            for (Map.Entry<Path, Path> backup : backups.entrySet()) {
                try {
                    Files.move(backup.getValue(), backup.getKey());
                    Files.delete(backup.getValue().getParent());
                } catch (IOException restoreError) {
                    // Preserve the backup if restoration fails.
                    e.addSuppressed(restoreError);
                }
            }
            throw e;
        } finally {
            if (committed) {
                for (Path backup : backups.values()) FileUtils.deleteDirectory(backup.getParent().toFile());
            }
        }
    }

    public static IResource receipt(Project project) {
        return project.getGameProjectResource().disableMinifyPath().changeExt(".lupackages");
    }

    public static byte[] serializeOutputs(Project project, List<Output> outputs) {
        StringBuilder text = new StringBuilder(PackageArtifacts.header(project));
        for (Output output : outputs) {
            text.append(output.directory()).append(' ').append(output.bundle()).append(' ')
                    .append(Base64.getEncoder().encodeToString(output.path().toString().getBytes(StandardCharsets.UTF_8))).append(' ')
                    .append(Base64.getEncoder().encodeToString(output.name().getBytes(StandardCharsets.UTF_8))).append(' ')
                    .append(output.inputSignature()).append(' ').append(output.contentHash()).append('\n');
        }
        return text.toString().getBytes(StandardCharsets.UTF_8);
    }

    public static List<Output> readOutputs(Project project) throws IOException {
        IResource receipt = receipt(project);
        if (!receipt.exists()) return List.of();
        return PackageArtifacts.parse(Path.of(project.getRootDirectory()), new String(receipt.getContent(), StandardCharsets.UTF_8));
    }

    /** Validate the entire artifact set before copying or starting any remote publication. */
    public static List<Output> validatedOutputs(Project project) throws IOException {
        if (!receipt(project).exists()) {
            throw new IOException("Live Update artifact receipt is missing; run build before bundling or publishing");
        }
        List<Output> outputs = readOutputs(project);
        for (Output output : outputs) {
            if (!isCurrent(output, project)) {
                throw new IOException("Live Update artifact is missing or modified; run build before bundling or publishing: " + output.path());
            }
        }
        return outputs;
    }

    static List<Output> parseOutputs(String content) throws IOException {
        List<Output> outputs = new ArrayList<>();
        for (String line : content.lines().toList()) {
            String[] fields = line.split(" ", -1);
            if (fields.length != 4 && fields.length != 6) throw new IOException("Invalid Live Update artifact receipt");
            outputs.add(new Output(Path.of(new String(Base64.getDecoder().decode(fields[2]), StandardCharsets.UTF_8)),
                    new String(Base64.getDecoder().decode(fields[3]), StandardCharsets.UTF_8), Boolean.parseBoolean(fields[0]), Boolean.parseBoolean(fields[1]),
                    fields.length == 6 ? fields[4] : "", fields.length == 6 ? fields[5] : ""));
        }
        return List.copyOf(outputs);
    }

    private static boolean isCurrent(Output output, Project project) throws IOException {
        return !output.contentHash().isEmpty() &&
                output.contentHash().equals(PackageCache.contentHash(output.path(), output.directory(), project::isBuildCanceled));
    }

    /** The task signature alone cannot detect deleted or modified external artifacts. */
    public static boolean outputsAreCurrent(Project project) throws IOException {
        for (Output output : readOutputs(project)) {
            if (!isCurrent(output, project)) return false;
        }
        return true;
    }

    public static void copyOutputs(Project project, Path bundleDirectory) throws IOException {
        copyOutputs(project, bundleDirectory, List.of());
    }

    public static void copyOutputs(Project project, Path bundleDirectory, Collection<Path> applicationOutputs) throws IOException {
        copyOutputs(project, bundleDirectory, applicationOutputs, Map.of());
    }

    /** Publish a rebuilt web application while retaining unchanged package files in its directory. */
    public static void publishBundle(Project project, Path applicationDirectory, Path bundleDirectory) throws IOException {
        Map<Path, Path> applicationOutputs = new LinkedHashMap<>();
        try (var files = Files.list(applicationDirectory)) {
            for (Path file : files.sorted().toList()) {
                applicationOutputs.put(bundleDirectory.resolve(file.getFileName()).toAbsolutePath().normalize(), file);
            }
        }
        copyOutputs(project, bundleDirectory, applicationOutputs.keySet(), applicationOutputs);
    }

    private static void collectObsoleteFiles(Path directory, Set<Path> retained, Set<Path> obsolete) throws IOException {
        if (!Files.isDirectory(directory)) return;
        try (var files = Files.list(directory)) {
            for (Path file : files.toList()) {
                if (retained.contains(file)) continue;
                if (retained.stream().anyMatch(path -> path.startsWith(file))) collectObsoleteFiles(file, retained, obsolete);
                else obsolete.add(file);
            }
        }
    }

    private static void copyOutputs(Project project, Path bundleDirectory, Collection<Path> applicationOutputs,
                                    Map<Path, Path> stagedApplication) throws IOException {
        bundleDirectory = bundleDirectory.toAbsolutePath().normalize();
        Path receipt = PackageArtifacts.bundleReceipt(project, bundleDirectory);
        Map<Path, Output> previous = new LinkedHashMap<>();
        for (Output output : PackageArtifacts.read(Path.of(project.getRootDirectory()), receipt)) previous.put(output.path(), output);
        Map<Path, Output> copies = new LinkedHashMap<>();
        List<Output> outputs = new ArrayList<>();
        for (Output output : validatedOutputs(project)) {
            if (!output.bundle()) continue;
            String hash = output.contentHash();
            Path destination = bundleDirectory.resolve(output.name()).toAbsolutePath().normalize();
            validateApplicationOutput(destination, applicationOutputs);
            outputs.add(new Output(destination, output.name(), output.directory(), true, output.inputSignature(), hash));
            if (output.path().equals(destination)) continue;
            if (destination.startsWith(output.path()) || output.path().startsWith(destination)) {
                throw new IOException("Overlapping Live Update source and bundle output: " + destination);
            }
            if (Files.exists(destination) && !previous.containsKey(destination)) {
                throw new IOException("Live Update artifact would overwrite an existing bundle output: " + destination);
            }
            copies.put(destination, output);
        }
        Set<Path> obsolete = new LinkedHashSet<>(previous.keySet());
        for (Output output : outputs) obsolete.remove(output.path());
        for (Path path : obsolete) validateApplicationOutput(path, applicationOutputs);
        if (!stagedApplication.isEmpty()) {
            Set<Path> retained = new LinkedHashSet<>(applicationOutputs);
            for (Output output : outputs) retained.add(output.path());
            collectObsoleteFiles(bundleDirectory, retained, obsolete);
        }
        // Stage changed artifacts and the receipt together. A cancelled delivery
        // restores the previous bundle and its ownership, so it can be retried.
        Path buildDirectory = Path.of(project.getRootDirectory(), project.getBuildDirectory());
        Files.createDirectories(buildDirectory);
        Path stage = Files.createTempDirectory(buildDirectory, ".liveupdate-bundle-");
        try {
            Map<Path, Path> stagedOutputs = new LinkedHashMap<>(stagedApplication);
            for (Map.Entry<Path, Output> copy : copies.entrySet()) {
                Path destination = copy.getKey();
                Output output = copy.getValue();
                String destinationHash = PackageCache.contentHash(destination, output.directory(), project::isBuildCanceled);
                if (output.contentHash().equals(destinationHash)) continue;
                Path staged = stage.resolve("artifact-" + stagedOutputs.size());
                // Preserve timestamps of unchanged members inside a changed folder.
                if (destinationHash != null && output.directory()) {
                    PackageCache.copy(destination, staged, project::isBuildCanceled);
                }
                PackageCache.copy(output.path(), staged, project::isBuildCanceled);
                stagedOutputs.put(destination, staged);
            }
            Path ownership = stage.resolve("ownership.outputs");
            Files.write(ownership, serializeOutputs(project, outputs));
            stagedOutputs.put(receipt, ownership);
            finalizeOutputs(project, stagedOutputs, obsolete);
        } finally {
            FileUtils.deleteDirectory(stage.toFile());
        }
    }

    private static void validateApplicationOutput(Path destination, Collection<Path> applicationOutputs) throws IOException {
        Path path = Path.of(destination.toString().toLowerCase(Locale.ROOT));
        for (Path applicationOutput : applicationOutputs) {
            Path application = Path.of(applicationOutput.toAbsolutePath().normalize().toString().toLowerCase(Locale.ROOT));
            if (path.startsWith(application) || application.startsWith(path)) {
                throw new IOException("Live Update artifact collides with an application output: " + destination);
            }
        }
    }

    @Override
    public void close() throws IOException {
        FileUtils.deleteDirectory(stage.toFile());
    }
}
