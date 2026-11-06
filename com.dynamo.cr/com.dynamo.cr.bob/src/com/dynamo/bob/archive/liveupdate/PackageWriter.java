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
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import org.apache.commons.io.FileUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.liveupdate.PackageArtifacts.Output;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompression;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompressionPreset;
import com.dynamo.liveupdate.proto.Manifest.PackageMetadata;
import com.dynamo.liveupdate.proto.Manifest.PackageOutput;
import com.dynamo.liveupdate.proto.Manifest.PackageRequirement;

/** Owns temporary payloads and the transaction that publishes a complete set of artifacts. */
public final class PackageWriter implements ArchiveBuilder.PreparedResourceConsumer, AutoCloseable {

    /** Finalized artifacts and per-package payload sizes for report generation. */
    public record WriteResult(List<Output> outputs, Map<String, List<Output>> packages,
            Map<String, Map<String, Long>> storedSizes) {}

    private record Delivery(PackageResolver.Package pkg,
            PackageProvider provider,
            List<PackageProvider.Artifact> artifacts) {}

    private final Project project;
    private final PackageResolver.Plan plan;
    private final Path stage;
    private record FileSettings(FileCompression compression, int level) {
        FileSettings(FileCompressionPreset preset) {
            this(preset.getFileCompression(), preset.getFileCompression() == FileCompression.LZ4 ? preset.getLz4Level() : 0);
        }
    }

    private final FileSettings baseCompression;
    private final Map<FileSettings, Map<String, ArchiveEntry>> prepared = new LinkedHashMap<>();
    private final Set<String> packaged = new TreeSet<>();

    /**
     * Creates a temporary payload staging area for the resolved plan. The caller must close the writer on success or
     * failure.
     */
    public PackageWriter(Project project, PackageResolver.Plan plan, FileCompressionPreset baseCompression) throws IOException {
        this.project = project;
        this.plan = plan;
        this.baseCompression = new FileSettings(baseCompression);
        prepared.put(this.baseCompression, new LinkedHashMap<>());
        Path buildDirectory = Path.of(project.getRootDirectory(), project.getBuildDirectory());
        Files.createDirectories(buildDirectory);
        stage = Files.createTempDirectory(buildDirectory, ".liveupdate-");
        Files.createDirectory(stage.resolve("payloads"));
        for (PackageResolver.Package pkg : plan.packages().values()) {
            packaged.addAll(pkg.resources());
        }
    }

    /**
     * Retains prepared payloads needed by any package, adding their Live Update header once per content hash.
     * Called by parallel archive workers; packages with the same compression reuse these bytes.
     */
    @Override
    public synchronized void accept(ArchiveEntry entry, byte[] payload) throws IOException {
        PackageArtifacts.checkCanceled(project::isBuildCanceled);
        String path = PackageResolver.normalize(entry.getRelativeFilename());
        if (!packaged.contains(path)) {
            return;
        }

        writePayload(entry, payload);
        prepared.get(baseCompression).put(path, entry);
    }

    /** Stores each payload once, even when different resource paths contain the same bytes. */
    private void writePayload(ArchiveEntry entry, byte[] payload) throws IOException {
        Path file = stage.resolve("payloads").resolve(entry.getHexDigest());
        if (!Files.exists(file)) {
            try (OutputStream output = Files.newOutputStream(file)) {
                output.write(entry.getHeader());
                output.write(payload);
            }
        }
    }

    /** Prepares only variants that differ from Base, caching them for packages with the same file compression. */
    private List<ArchiveEntry> prepare(PackageResolver.Package pkg, FileCompressionPreset preset) throws IOException, CompileExceptionError {
        FileSettings compression = new FileSettings(preset);
        Map<String, ArchiveEntry> entries = prepared.computeIfAbsent(compression, key -> new LinkedHashMap<>());
        List<ArchiveEntry> result = new ArrayList<>();
        for (String path : pkg.resources().stream().sorted().toList()) {
            PackageArtifacts.checkCanceled(project::isBuildCanceled);
            ArchiveEntry entry = entries.get(path);
            if (entry == null) {
                ArchiveEntry original = prepared.get(baseCompression).get(path);
                if (original == null) {
                    throw new IOException("Missing prepared resource: " + path);
                }
                Set<Task.OutputFlags> flags = project.getOutputFlags(original.getFilename());
                boolean compress = compression.compression() == FileCompression.LZ4
                        && (flags == null || !flags.contains(Task.OutputFlags.UNCOMPRESSED));
                String root = Path.of(project.getRootDirectory(), project.getBuildDirectory()).toAbsolutePath().toString();
                entry = new ArchiveEntry(root, original.getFilename(), compress, original.isEncrypted());
                writePayload(entry, project.getArchiveBuilder().prepareResource(entry, preset.getLz4Level()));
                entries.put(path, entry);
            }
            result.add(entry);
        }
        return List.copyOf(result);
    }

    /**
     * Validates all output declarations, then prepares and writes selected packages with package-local manifests.
     * Stages fresh artifacts and ownership together before installation, removing obsolete owned manifest outputs.
     *
     * @param manifests configured manifest builder, including hashing and engine-version settings
     * @param buildOutputs reserved build outputs that package destinations must not overlap
     * @param selection package names to write; other packages remain available in dependency metadata
     * @return the finalized artifacts and their package associations
     * @throws IOException on I/O failure or cancellation; installation attempts to restore previous outputs
     * @throws CompileExceptionError on invalid providers, settings, or output collisions
     */
    public WriteResult write(ManifestBuilder manifests, Collection<IResource> buildOutputs, Set<String> selection) throws IOException, CompileExceptionError {
        Map<String, PackageProvider> providers = PackageProviders.discover(Project.getClassLoaderScanner());
        PackageProvider.Context context = PackageProviders.context(project, plan, providers);
        Path legacyOutput = PackageBuilder.manifestPath(project).isEmpty() ? PackageBuilder.legacyOutput(project) : null;
        boolean legacyPlacement = legacyOutput != null;
        boolean bundle = false;
        Path root;
        if (legacyPlacement) {
            root = legacyOutput.getParent();
            PublisherSettings settings = project.getPublisher().getPublisherSettings();
            bundle = settings.getMode() == PublisherSettings.PublishMode.Zip
                    ? settings.getSaveZipInBundleFolder()
                    : settings.getSaveFolderInBundleFolder();
        } else {
            root = PackageBuilder.outputDirectory(project);
        }

        Map<Path, Output> previousOutputs = new LinkedHashMap<>();
        for (Output output : PackageArtifacts.readBuildOutputs(project)) {
            previousOutputs.put(output.path(), output);
        }
        // Recognize shared destinations, but only retire outputs from this build directory.
        Map<Path, Output> knownOutputs = new LinkedHashMap<>();
        for (Output output : PackageArtifacts.readProjectBuildOutputs(project)) {
            knownOutputs.put(output.path(), output);
        }
        knownOutputs.putAll(previousOutputs);
        List<Delivery> deliveries = new ArrayList<>();
        Map<Path, Path> stagedOutputs = new LinkedHashMap<>();
        List<Output> outputs = new ArrayList<>();
        Map<String, List<Output>> packageOutputs = new LinkedHashMap<>();
        Map<String, Map<String, Long>> storedSizes = new LinkedHashMap<>();

        // Reserve build outputs before they exist, including this writer's ownership index.
        List<Path> reservedOutputs = new ArrayList<>(List.of(PackageArtifacts.buildIndex(project)));
        for (IResource output : buildOutputs) {
            reservedOutputs.add(Path.of(output.getAbsPath()));
        }
        List<Path> occupied = new ArrayList<>();
        for (Path output : reservedOutputs) {
            Path path = resolveExistingPath(output.toAbsolutePath().normalize());
            occupied.add(Path.of(path.toString().toLowerCase(Locale.ROOT)));
        }

        List<PackageOutput> references = new ArrayList<>();
        for (PackageResolver.Package pkg : plan.packages().values()) {
            context.checkCanceled();
            String name = pkg.description().getName();
            String providerId = PackageResolver.containerPreset(context.manifest(), pkg.description().getContainerPreset()).getProvider();
            PackageProvider provider = providers.get(providerId);
            List<PackageProvider.Artifact> artifacts = List.copyOf(provider.describeOutputs(context, pkg));
            if (legacyOutput != null) {
                String filename = legacyOutput.getFileName().toString();
                artifacts = List.of(new PackageProvider.Artifact(filename, !providerId.equals("zip")));
            }

            PackageOutput.Builder reference = PackageOutput.newBuilder()
                    .setPackage(name)
                    .setProvider(providerId);
            for (PackageProvider.Artifact artifact : artifacts) {
                Path relative = Path.of(artifact.name());
                if (relative.isAbsolute()
                        || relative.getNameCount() == 0
                        || !relative.normalize().equals(relative)
                        || relative.startsWith("..")
                        || artifact.name().isBlank()
                        || artifact.name().contains("\\")) {
                    throw new CompileExceptionError("Invalid package artifact path: " + artifact.name());
                }

                Path destination = root.resolve(relative).normalize();
                Path resolvedDestination = resolveExistingPath(destination);
                Path collisionKey = Path.of(resolvedDestination.toString().toLowerCase(Locale.ROOT));
                for (Path previous : occupied) {
                    if (collisionKey.startsWith(previous) || previous.startsWith(collisionKey)) {
                        throw new CompileExceptionError("Colliding Live Update outputs: " + destination);
                    }
                }
                occupied.add(collisionKey);

                for (String source : plan.sourceOutputs().keySet()) {
                    Path sourcePath = Path.of(project.getRootDirectory())
                            .resolve(source.substring(1)).toAbsolutePath().normalize();
                    if (resolveExistingPath(sourcePath).startsWith(resolvedDestination)) {
                        throw new CompileExceptionError("Package output would overwrite a source resource: " + destination);
                    }
                }

                Output previous = findPreviousOutput(knownOutputs, destination);
                if (previous != null && Files.exists(destination) && project.hasOption("package")
                        && selection.contains(name) && !previous.packageName().isEmpty()
                        && !selection.contains(previous.packageName())) {
                    throw new CompileExceptionError("Package output belongs to unselected package '"
                            + previous.packageName() + "': " + destination);
                }
                if (Files.exists(destination) && !legacyPlacement
                        && (previous == null || (previous.legacy() && previous.directory()))) {
                    throw new CompileExceptionError(
                            "Package output would overwrite an existing file or directory: " + destination);
                }
                if (legacyPlacement && Files.exists(destination, LinkOption.NOFOLLOW_LINKS)
                        && (Files.isSymbolicLink(destination) || artifact.directory() != Files.isDirectory(destination))) {
                    throw new CompileExceptionError("Package output has a different file type: " + destination);
                }
                reference.addArtifacts(artifact.name());
            }
            references.add(reference.build());
            deliveries.add(new Delivery(pkg, provider, artifacts));
        }

        for (Delivery delivery : deliveries) {
            if (!selection.contains(delivery.pkg().description().getName())) {
                continue;
            }
            context.checkCanceled();
            int firstOutput = outputs.size();
            PackageResolver.Package pkg = delivery.pkg();
            FileCompressionPreset preset = PackageResolver.fileCompressionPreset(context.manifest(), pkg.description().getFileCompressionPreset());
            List<ArchiveEntry> entries = prepare(pkg, preset);
            Map<String, Long> sizes = new LinkedHashMap<>();
            for (ArchiveEntry entry : entries) {
                sizes.put(entry.getRelativeFilename(), (long)(entry.isCompressed() ? entry.getCompressedSize() : entry.getSize()));
            }
            storedSizes.put(pkg.description().getName(), Map.copyOf(sizes));
            PackageMetadata.Builder metadata = PackageMetadata.newBuilder()
                    .setName(pkg.description().getName())
                    .addAllOutputs(references);
            Map<String, List<String>> requirements = plan.requirements().get(pkg.description().getName());
            for (Map.Entry<String, List<String>> requirement : requirements.entrySet()) {
                metadata.addRequirements(PackageRequirement.newBuilder()
                        .setUrlHash(MurmurHash.hash64(requirement.getKey()))
                        .addAllPackages(requirement.getValue()));
            }

            List<PackageProvider.PreparedResource> resources = new ArrayList<>();
            for (ArchiveEntry entry : entries) {
                resources.add(new PackageProvider.PreparedResource(new PackageProvider.ResourceInfo(entry),
                        stage.resolve("payloads").resolve(entry.getHexDigest())));
            }
            resources = List.copyOf(resources);
            byte[] manifest = manifests.buildPackageManifest(entries, metadata.build());
            Path packageStage = stage.resolve("package-" + packageOutputs.size());
            Files.createDirectory(packageStage);
            delivery.provider().write(context, pkg, resources, manifest, packageStage);
            if (legacyOutput != null) {
                String producedName = delivery.provider().describeOutputs(context, pkg).getFirst().name();
                Path produced = packageStage.resolve(producedName);
                Path named = packageStage.resolve(delivery.artifacts().getFirst().name());
                if (!produced.equals(named)) {
                    Files.move(produced, named);
                }
            }

            for (PackageProvider.Artifact artifact : delivery.artifacts()) {
                Path destination = root.resolve(artifact.name());
                Path output = packageStage.resolve(artifact.name());
                if (legacyPlacement && artifact.directory() && Files.isDirectory(destination)) {
                    preserveLegacyFolder(destination, output);
                }
                if (!isValidOutput(project, output, artifact.directory())) {
                    throw new IOException("Provider did not produce a regular, non-symlink output: " + artifact.name());
                }
                stagedOutputs.put(destination, output);
                outputs.add(new Output(destination, artifact.name(), artifact.directory(), bundle, legacyPlacement, pkg.description().getName()));
            }
            packageOutputs.put(pkg.description().getName(), List.copyOf(outputs.subList(firstOutput, outputs.size())));
        }

        context.checkCanceled();
        Set<Path> obsolete = new LinkedHashSet<>(previousOutputs.keySet());
        removeCurrentOutputs(obsolete, previousOutputs, outputs);

        List<Output> ownedOutputs = new ArrayList<>(outputs);
        // Legacy releases are retained when their destination changes; folders can also contain unowned files.
        for (Output previous : previousOutputs.values()) {
            if ((previous.legacy() || (project.hasOption("package") && !selection.contains(previous.packageName())))
                    && obsolete.remove(previous.path())) {
                ownedOutputs.add(previous);
            }
        }

        Path ownership = stage.resolve("ownership.outputs");
        Files.write(ownership, PackageArtifacts.serializeOutputs(project, ownedOutputs));
        stagedOutputs.put(PackageArtifacts.buildIndex(project), ownership);
        finalizeOutputs(project, stagedOutputs, obsolete);
        return new WriteResult(List.copyOf(outputs), Map.copyOf(packageOutputs), Map.copyOf(storedSizes));
    }

    /** Carries forward files absent from the new staged folder, preserving the legacy publisher's merge behavior. */
    private void preserveLegacyFolder(Path source, Path destination) throws IOException {
        try (var files = Files.walk(source)) {
            for (Path file : files.toList()) {
                PackageArtifacts.checkCanceled(project::isBuildCanceled);
                Path target = destination.resolve(source.relativize(file));
                if (Files.isDirectory(file, LinkOption.NOFOLLOW_LINKS)) {
                    Files.createDirectories(target);
                } else if (!Files.exists(target, LinkOption.NOFOLLOW_LINKS)) {
                    Files.copy(file, target, StandardCopyOption.COPY_ATTRIBUTES, LinkOption.NOFOLLOW_LINKS);
                }
            }
        }
    }

    /** Resolve existing ancestors as well, so aliases cannot hide a source beneath a new output directory. */
    private static Path resolveExistingPath(Path path) throws IOException {
        if (Files.exists(path)) {
            return path.toRealPath();
        }
        Path parent = path.getParent();
        return parent == null ? path : resolveExistingPath(parent).resolve(path.getFileName());
    }

    /** Finds existing ownership by exact path or a case-only rename referring to the same physical file. */
    private static Output findPreviousOutput(Map<Path, Output> previous, Path path) throws IOException {
        Output exact = previous.get(path);
        if (exact != null) {
            return exact;
        }
        if (Files.exists(path, LinkOption.NOFOLLOW_LINKS)) {
            for (Output output : previous.values()) {
                if (output.path().toString().equalsIgnoreCase(path.toString())
                        && Files.exists(output.path(), LinkOption.NOFOLLOW_LINKS)
                        && Files.isSameFile(output.path(), path)) {
                    return output;
                }
            }
        }
        return null;
    }

    /** Removes retained or replaced artifacts from the obsolete set, accounting for case-only renames. */
    private static void removeCurrentOutputs(Set<Path> obsolete, Map<Path, Output> previous, List<Output> outputs) throws IOException {
        for (Output output : outputs) {
            Output replaced = findPreviousOutput(previous, output.path());
            if (replaced != null) {
                obsolete.remove(replaced.path());
            }
        }
    }

    /**
     * Installs destination-to-staged-path entries and removes obsolete outputs, backing up affected paths first.
     * On I/O failure or cancellation, attempts rollback; failed restorations keep their backups and become suppressed errors.
     */
    private static void finalizeOutputs(Project project, Map<Path, Path> outputs, Set<Path> obsolete) throws IOException {
        Set<Path> affected = new LinkedHashSet<>(obsolete);
        affected.addAll(outputs.keySet());
        List<Path> roots = new ArrayList<>();
        List<Path> sortedPaths = affected.stream()
                .sorted(Comparator.comparingInt(Path::getNameCount))
                .toList();
        for (Path path : sortedPaths) {
            if (roots.stream().noneMatch(path::startsWith)) {
                roots.add(path);
            }
        }

        Map<Path, Path> backups = new LinkedHashMap<>();
        boolean installing = false;
        boolean committed = false;
        try {
            for (Path destination : roots) {
                PackageArtifacts.checkCanceled(project::isBuildCanceled);
                if (Files.exists(destination)) {
                    Path backupDirectory = Files.createTempDirectory(destination.getParent(), ".liveupdate-backup-");
                    Path backup = backupDirectory.resolve("previous");
                    Files.move(destination, backup);
                    backups.put(destination, backup);
                }
            }

            installing = true;
            for (Map.Entry<Path, Path> output : outputs.entrySet()) {
                PackageArtifacts.checkCanceled(project::isBuildCanceled);
                Path destination = output.getKey();
                Files.createDirectories(destination.getParent());
                if (Files.isDirectory(output.getValue())) {
                    FileUtils.moveDirectory(output.getValue().toFile(), destination.toFile());
                } else {
                    Files.move(output.getValue(), destination, StandardCopyOption.REPLACE_EXISTING);
                }
            }
            PackageArtifacts.checkCanceled(project::isBuildCanceled);
            committed = true;
        } catch (IOException e) {
            if (installing) {
                for (Path path : roots) {
                    try {
                        delete(path);
                    } catch (IOException cleanupError) {
                        e.addSuppressed(cleanupError);
                    }
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
                for (Path backup : backups.values()) {
                    FileUtils.deleteDirectory(backup.getParent().toFile());
                }
            }
        }
    }

    /** Validate the entire artifact set before copying into the bundle. */
    private static List<Output> validatedOutputs(Project project) throws IOException {
        if (!PackageArtifacts.resource(project).exists()) {
            throw new IOException("Live Update artifact index is missing; run bundle again");
        }
        List<Output> outputs = PackageArtifacts.readOutputs(project);
        for (Output output : outputs) {
            if (!isValidOutput(project, output.path(), output.directory())) {
                throw new IOException("Live Update artifact is missing or has an invalid file type; run bundle again: " + output.path());
            }
        }
        return outputs;
    }

    /** Checks provider declarations and rejects symbolic links or unsupported entries without reading payloads. */
    private static boolean isValidOutput(Project project, Path path, boolean directory) throws IOException {
        PackageArtifacts.checkCanceled(project::isBuildCanceled);
        if (!directory) {
            return Files.isRegularFile(path, LinkOption.NOFOLLOW_LINKS);
        }
        if (!Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) {
            return false;
        }
        try (var files = Files.walk(path)) {
            for (Path file : files.toList()) {
                PackageArtifacts.checkCanceled(project::isBuildCanceled);
                if (!Files.isRegularFile(file, LinkOption.NOFOLLOW_LINKS)
                        && !Files.isDirectory(file, LinkOption.NOFOLLOW_LINKS)) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Validates built artifacts and copies those marked for bundling into fresh staging paths.
     * Stages all copies and the bundle ownership index together, removing obsolete copies only during finalization.
     */
    public static void copyOutputs(Project project, Path bundleDirectory) throws IOException {
        bundleDirectory = bundleDirectory.toAbsolutePath().normalize();
        Path index = PackageArtifacts.bundleIndex(project, bundleDirectory);
        Map<Path, Output> previous = new LinkedHashMap<>();
        for (Output output : PackageArtifacts.read(Path.of(project.getRootDirectory()), index)) {
            previous.put(output.path(), output);
        }

        Map<Path, Path> copies = new LinkedHashMap<>();
        List<Output> outputs = new ArrayList<>();
        for (Output output : validatedOutputs(project)) {
            if (!output.bundle()) {
                continue;
            }
            Path destination = bundleDirectory.resolve(output.name()).toAbsolutePath().normalize();
            outputs.add(new Output(destination, output.name(), output.directory(), true, output.legacy(), output.packageName()));
            if (output.path().equals(destination)) {
                continue;
            }
            if (destination.startsWith(output.path()) || output.path().startsWith(destination)) {
                throw new IOException("Overlapping Live Update source and bundle output: " + destination);
            }
            if (Files.exists(destination, LinkOption.NOFOLLOW_LINKS) && findPreviousOutput(previous, destination) == null) {
                boolean legacyDestination = output.legacy() && (output.directory()
                        ? Files.isDirectory(destination, LinkOption.NOFOLLOW_LINKS)
                        : Files.isRegularFile(destination, LinkOption.NOFOLLOW_LINKS));
                if (!legacyDestination) {
                    throw new IOException("Live Update artifact would overwrite an existing bundle output: " + destination);
                }
            }
            copies.put(destination, output.path());
        }

        if (outputs.isEmpty() && previous.isEmpty()) {
            return;
        }

        Set<Path> obsolete = new LinkedHashSet<>(previous.keySet());
        removeCurrentOutputs(obsolete, previous, outputs);
        if (project.hasOption("package")) {
            Set<String> selected = Set.copyOf(List.of(project.option("package", "").split(",")));
            List<Output> available = PackageArtifacts.readBuildOutputs(project);
            for (Output output : previous.values()) {
                if (!selected.contains(output.packageName()) && obsolete.remove(output.path())) {
                    outputs.add(output);
                    // Application bundlers may recreate their directory. Restore unselected copies from
                    // their existing package artifacts, without rebuilding the packages themselves.
                    if (!Files.exists(output.path(), LinkOption.NOFOLLOW_LINKS)) {
                        for (Output source : available) {
                            if (source.packageName().equals(output.packageName()) && source.name().equals(output.name())) {
                                if (!isValidOutput(project, source.path(), source.directory())) {
                                    throw new IOException("Live Update artifact is missing or has an invalid file type; run bundle again: " + source.path());
                                }
                                copies.put(output.path(), source.path());
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Stage all artifacts and the index together. A cancelled delivery
        // restores the previous package copies and their ownership, so it can be retried.
        Path buildDirectory = Path.of(project.getRootDirectory(), project.getBuildDirectory());
        Files.createDirectories(buildDirectory);
        Path stage = Files.createTempDirectory(buildDirectory, ".liveupdate-bundle-");
        try {
            Map<Path, Path> stagedOutputs = new LinkedHashMap<>();
            for (Map.Entry<Path, Path> copy : copies.entrySet()) {
                Path staged = stage.resolve("artifact-" + stagedOutputs.size());
                copy(project, copy.getValue(), staged);
                stagedOutputs.put(copy.getKey(), staged);
            }

            Path ownership = stage.resolve("ownership.outputs");
            Files.write(ownership, PackageArtifacts.serializeOutputs(project, outputs));
            stagedOutputs.put(index, ownership);
            finalizeOutputs(project, stagedOutputs, obsolete);
        } finally {
            FileUtils.deleteDirectory(stage.toFile());
        }
    }

    /** Deletes an artifact tree or file if present, without following a root symlink as a directory. */
    private static void delete(Path path) throws IOException {
        if (Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) {
            FileUtils.deleteDirectory(path.toFile());
        } else {
            Files.deleteIfExists(path);
        }
    }

    /** Copies every file into fresh staging, checking cancellation between files. */
    private static void copy(Project project, Path source, Path destination) throws IOException {
        PackageArtifacts.checkCanceled(project::isBuildCanceled);
        if (Files.isDirectory(source)) {
            Files.createDirectories(destination);
            try (var entries = Files.list(source)) {
                for (Path entry : entries.toList()) {
                    copy(project, entry, destination.resolve(entry.getFileName()));
                }
            }
        } else {
            Files.createDirectories(destination.getParent());
            Files.copy(source, destination, StandardCopyOption.COPY_ATTRIBUTES);
        }
    }

    /** Deletes temporary payloads and unfinished provider outputs; finalized artifacts remain in their destinations. */
    @Override
    public void close() throws IOException {
        FileUtils.deleteDirectory(stage.toFile());
    }
}
