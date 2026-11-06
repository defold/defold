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
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import java.util.UUID;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.IProgress;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.pipeline.graph.BuildGraph;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.util.ReportGenerator;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompression;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ContainerPreset;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompressionPreset;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.PackageDesc;
import com.google.protobuf.TextFormat;

/** Adapts actual Bob tasks and compiled resource references to the pure resolver. */
public final class PackageBuilder {

    // Project creates task builders reflectively, so this implementation must be public.
    public static final class CopyResource extends CopyBuilder {

        /** Declares a passthrough build output for a selected custom file without a registered compiler. */
        @Override
        public Task create(IResource input) {
            return Task.newBuilder(this)
                    .setName("Copy package resource")
                    .addInput(input)
                    .addOutput(input.output())
                    .build();
        }
    }

    private static final Pattern VARIABLE = Pattern.compile(
            "\\$\\{([A-Za-z_][A-Za-z0-9_]*)\\}|\\$([A-Za-z_][A-Za-z0-9_]*)|%([A-Za-z_][A-Za-z0-9_]*)%");

    /**
     * Expands ${NAME}, $NAME, and %NAME% variables, resolving relative paths against the project root.
     * Returns an absolute normalized path without creating it.
     *
     * @param environment variable lookup returning null for undefined names
     * @throws IllegalArgumentException if a variable is undefined or the expanded path is blank
     */
    public static Path resolveOutputDirectory(String directory, Path projectRoot, Function<String, String> environment) {
        Matcher matcher = VARIABLE.matcher(directory);
        StringBuilder expanded = new StringBuilder();
        while (matcher.find()) {
            String name = matcher.group(1);
            if (name == null) {
                name = matcher.group(2);
            }
            if (name == null) {
                name = matcher.group(3);
            }
            String value = environment.apply(name);
            if (value == null) {
                throw new IllegalArgumentException(
                        "Undefined environment variable '" + name + "' in Live Update output directory");
            }
            matcher.appendReplacement(expanded, Matcher.quoteReplacement(value));
        }
        matcher.appendTail(expanded);
        if (expanded.toString().isBlank()) {
            throw new IllegalArgumentException("Live Update output directory must not be empty");
        }
        return projectRoot.resolve(expanded.toString()).toAbsolutePath().normalize();
    }

    private final IResource manifestResource;
    private final ManifestDesc manifest;
    private final List<String> inventory;
    private List<Task> resourceTasks;
    private BuildGraph previousGraph;
    private Set<String> buildSources = Set.of();
    private final Map<String, Task> selectedTasks = new LinkedHashMap<>();

    /** Explicit package selection enables external placement; legacy callers retain their existing yes/no setting. */
    public static boolean splitPackages(Project project) {
        return project.hasOption("package") || project.option("all-packages", "false").equals("true")
                || project.option("liveupdate", "false").equals("true");
    }

    /** Returns the selected outputs, validating names before compilation or provider writes. */
    public Set<String> selectedPackages(Project project) throws CompileExceptionError {
        Set<String> available = new LinkedHashSet<>();
        available.add("base");
        if (manifestPath(project).isEmpty()) {
            available.add("liveupdate");
        } else {
            for (PackageDesc pkg : manifest.getPackagesList()) {
                available.add(pkg.getName());
            }
        }
        if (project.hasOption("package")) {
            if (project.option("all-packages", "false").equals("true")) {
                throw new CompileExceptionError("--package cannot be combined with --all-packages");
            }
            Set<String> selected = new LinkedHashSet<>(List.of(project.option("package", "").split(",", -1)));
            for (String name : selected) {
                if (!available.contains(name)) {
                    throw new CompileExceptionError("Unknown package '" + name + "'. Available packages: " + String.join(", ", available));
                }
            }
            return selected;
        }
        return splitPackages(project) ? available : Set.of("base");
    }

    /**
     * Returns the selected .archivemanifest path, or an empty path for legacy publisher settings.
     * The package-manifest override lets conversion compare legacy and manifest plans without editing game.project.
     */
    public static String manifestPath(Project project) {
        String configuredPath = project.getProjectProperties().getStringValue("archive", "manifest", "");
        return project.option("package-manifest", configuredPath).trim();
    }

    /** Parses UTF-8 protobuf text; malformed or incomplete manifests fail rather than being silently repaired. */
    public static ManifestDesc parse(byte[] content) throws IOException {
        ManifestDesc.Builder builder = ManifestDesc.newBuilder();
        TextFormat.merge(new String(content, StandardCharsets.UTF_8), builder);
        return builder.build();
    }

    /** Uses the manifest's Base setting, retaining project.compress_archive for legacy projects. */
    public FileCompressionPreset baseCompression(Project project) throws CompileExceptionError {
        return manifestPath(project).isEmpty() ? legacyCompression(project)
                : PackageResolver.fileCompressionPreset(manifest, manifest.getBaseFileCompressionPreset());
    }

    /** Copies legacy per-file compression without inheriting container settings. */
    static FileCompressionPreset legacyCompression(Project project) {
        return FileCompressionPreset.newBuilder().setName("legacy")
                .setFileCompression(project.getProjectProperties().getBooleanValue("project", "compress_archive", true)
                        ? FileCompression.LZ4 : FileCompression.NONE)
                .build();
    }

    /** Copies the selected legacy container and its compression level when packages are exported. */
    static ContainerPreset legacyContainer(Project project) throws CompileExceptionError {
        ContainerPreset.Builder preset = ContainerPreset.newBuilder().setName("legacy");
        if (project.getPublisher() != null && splitPackages(project)) {
            var settings = project.getPublisher().getPublisherSettings();
            if (settings.getMode() == PublisherSettings.PublishMode.Zip) {
                preset.setZipLevel(settings.getCompressionLevel());
            } else {
                preset.setProvider("folder");
            }
        }
        return preset.build();
    }

    /**
     * Builds an isolated source snapshot and returns Bob's exact package plan without writing provider artifacts.
     * Platform and variant default to host/debug. Build intermediates stay in snapshot/build; the caller owns snapshot cleanup.
     *
     * @param snapshot project sources, including current library files and any unsaved editor content
     * @param manifestPath manifest override; an empty string selects the legacy Exclude workflow
     * @param options additional Bob options, subject to the forced Live Update and preview settings
     * @param classLoader loader for Bob and pipeline plugins
     * @param progress build progress and cancellation callbacks
     */
    public static PackageResolver.Plan buildPlan(Path snapshot, String manifestPath, Map<String, String> options,
            ClassLoader classLoader, IProgress progress) throws IOException, CompileExceptionError, MultipleCompileException {
        try (Project project = new Project(classLoader, new DefaultFileSystem(), snapshot.toString(), "build")) {
            options.forEach(project::setOption);
            project.setOption("platform", options.getOrDefault("platform", Platform.getHostPlatform().getPair()));
            project.setOption("variant", options.getOrDefault("variant", "debug"));
            project.setOption("liveupdate", "true");
            project.setOption("package-preview", "true");
            project.setOption("package-manifest", manifestPath);
            // Library resources are already captured in the snapshot by the editor.
            project.mount(new ClassLoaderResourceScanner(), List.of());
            for (TaskResult task : project.build(progress, "build")) {
                if (!task.isOk()) {
                    throw new CompileExceptionError(task.getTask().input(0), task.getLineNumber(),
                            task.getMessage(), task.getException());
                }
            }
            BundleHelper.throwIfCanceled(progress);
            PackageResolver.Plan plan = project.getLiveUpdatePlan();
            if (plan == null) {
                throw new IOException("Build did not produce a Live Update package plan");
            }
            return plan;
        }
    }

    /** Resolves the manifest output root against the project directory. */
    static Path outputDirectory(Project project) throws IOException, CompileExceptionError {
        IResource resource = project.getResource(manifestPath(project));
        try {
            String directory = parse(resource.getContent()).getOutputDirectory();
            return resolveOutputDirectory(directory, Path.of(project.getRootDirectory()), project::getSystemEnv);
        } catch (IllegalArgumentException e) {
            throw new CompileExceptionError(resource, 0, e.getMessage(), e);
        }
    }

    /** Preserve legacy destinations, including generated names across incremental builds. */
    static Path legacyOutput(Project project) throws IOException, CompileExceptionError {
        PublisherSettings settings = project.getPublisher().getPublisherSettings();
        Path projectRoot = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        boolean zip = settings.getMode() != PublisherSettings.PublishMode.Folder;
        String directory = zip ? settings.getZipFilepath() : settings.getOutputDirectory();
        Path root = projectRoot.resolve(directory == null ? "" : directory).normalize();
        String name = zip ? settings.getZipFilename() : settings.getOutputFolderName();
        if (name == null || name.isEmpty()) {
            for (PackageArtifacts.Output previous : PackageArtifacts.readBuildOutputs(project)) {
                if (previous.directory() != zip && previous.path().getParent().equals(root)) {
                    return previous.path();
                }
            }
            String platform = project.option("platform", "generic");
            if (zip) {
                name = "defold.resourcepack_" + platform + "_" + UUID.randomUUID() + ".zip";
            } else {
                name = "defold_resources_" + platform + System.currentTimeMillis() / 1000;
            }
        } else if (zip && !name.endsWith(".zip")) {
            name += ".zip";
        }
        return root.resolve(name).normalize();
    }

    /** Reads current manifest rules and source inventory without creating compiler tasks. */
    public PackageBuilder(Project project) throws IOException, CompileExceptionError {
        if (manifestPath(project).isEmpty()) {
            manifestResource = project.getGameProjectResource();
            manifest = ManifestDesc.getDefaultInstance();
            resourceTasks = List.copyOf(project.getTasks());
            inventory = resourceTasks.stream()
                    .flatMap(resourceTask -> resourceTask.getInputs().stream())
                    .filter(input -> !input.isOutput())
                    .map(input -> PackageResolver.normalize(input.getPath()))
                    .distinct()
                    .sorted()
                    .toList();
            return;
        }

        manifestResource = project.getResource(manifestPath(project));
        if (!manifestResource.exists()) {
            throw new CompileExceptionError(manifestResource, 0, "Live Update manifest does not exist");
        }
        try {
            manifest = parse(manifestResource.getContent());
        } catch (Exception e) {
            throw new CompileExceptionError(manifestResource, 0, "Invalid Live Update manifest: " + e.getMessage(), e);
        }

        Set<String> paths = new TreeSet<>();
        project.findResourcePaths("", paths);

        // Outputs from previous builds are not authoring resources, even when their destination is in the project.
        Path buildDirectory = Path.of(project.getRootDirectory(), project.getBuildDirectory()).toAbsolutePath().normalize();
        Set<Path> reportOutputs = ReportGenerator.outputPaths(project);
        paths.removeIf(path -> {
            Path absolute = Path.of(project.getResource(path).getAbsPath()).toAbsolutePath().normalize();
            return absolute.startsWith(buildDirectory) || reportOutputs.contains(absolute);
        });
        Set<String> generatedPaths = PackageArtifacts.generatedPaths(Path.of(project.getRootDirectory()));
        paths.removeIf(path -> PackageArtifacts.isGeneratedPath(path, generatedPaths));
        for (PackageArtifacts.Output output : PackageArtifacts.readBuildOutputs(project)) {
            paths.removeIf(path -> Path.of(project.getResource(path).getAbsPath()).normalize().startsWith(output.path()));
        }

        inventory = paths.stream().map(PackageResolver::normalize).toList();
        PackageResolver.Selection selection = PackageResolver.select(manifest, inventory);
        validate(selection.diagnostics());
        for (List<String> matches : selection.matches().values()) {
            for (String source : matches) {
                if (project.getResource(source).equals(project.getGameProjectResource()) || source.endsWith(".archivemanifest")) {
                    throw new CompileExceptionError(manifestResource, 0, "Package rule selects build configuration: " + source);
                }
            }
        }

        resourceTasks = List.of();
    }

    /** Selects source roots before creating tasks. Returns whether bootstrap and custom roots must also be built. */
    public boolean prepareBuild(Project project, Task.TaskBuilder task, Collection<String> bootstrap,
            Collection<String> custom) throws IOException, CompileExceptionError {
        Set<String> names = selectedPackages(project);
        PackageResolver.Selection selection = PackageResolver.select(manifest, inventory);
        if (manifestPath(project).isEmpty()) {
            task.addExtraCacheKey("legacy-packages-v3");
        } else {
            task.addInput(manifestResource);
            // Newly matching files invalidate an otherwise unchanged build.
            task.addExtraCacheKey(String.join("\n", inventory));
        }
        Set<String> sources = new TreeSet<>();
        for (List<String> matches : selection.matches().values()) {
            sources.addAll(matches);
        }
        buildSources = sources;
        if (!project.hasOption("package")) {
            return true;
        }

        BuildGraph previous;
        try {
            previous = BuildGraph.load(project);
            previous.validate(project);
            if (!previous.covers(bootstrap, custom, sources)) {
                return true;
            }
        } catch (IOException | CompileExceptionError e) {
            // Establish the complete graph first; normal compiler caching still applies.
            return true;
        }

        Set<String> selected = new TreeSet<>();
        if (manifestPath(project).isEmpty()) {
            if (names.contains("liveupdate")) {
                ResourceGraph graph = previous.graph(project);
                Set<String> excluded = graph.createExcludedResourcesList();
                Map<String, String> primarySources = previous.primarySources();
                for (ResourceNode node : graph.getNodes()) {
                    if (node.checkType(ResourceNode.Type.ExcludedCollection) && excluded.contains(node.getPath())) {
                        String source = primarySources.get(node.getPath());
                        if (source == null) {
                            return true;
                        }
                        selected.add(source);
                    }
                }
            }
        } else {
            // Referenced packages contribute to Add and Remove rules without becoming selected bundle outputs.
            Set<String> referenced = new LinkedHashSet<>(names);
            List<String> pending = new ArrayList<>(names);
            for (int i = 0; i < pending.size(); ++i) {
                for (PackageDesc pkg : manifest.getPackagesList()) {
                    if (pkg.getName().equals(pending.get(i))) {
                        for (var rule : pkg.getRulesList()) {
                            if (rule.hasPackage() && referenced.add(rule.getPackage())) {
                                pending.add(rule.getPackage());
                            }
                        }
                    }
                }
            }
            for (var matches : selection.matches().entrySet()) {
                if (referenced.contains(matches.getKey().packageName())) {
                    selected.addAll(matches.getValue());
                }
            }
        }
        previousGraph = previous;
        buildSources = selected;
        return names.contains("base");
    }

    /** Creates only the selected roots and their compiler dependencies, then retains their source/output declarations. */
    public void createTasks(Project project) throws CompileExceptionError {
        Set<String> custom = new LinkedHashSet<>();
        for (String path : CopyCustomResourcesBuilder.getCustomResourcePaths(project)) {
            custom.add(PackageResolver.normalize(path));
        }
        if (buildSources.stream().anyMatch(custom::contains)) {
            project.createTask(project.getGameProjectResource(), CopyCustomResourcesBuilder.class);
        }

        Set<Task> existingTasks = Set.copyOf(project.getTasks());
        for (String path : buildSources) {
            IResource source = project.getResource(path);
            Task selectedTask;
            if (custom.contains(path)) {
                // The existing custom-resource task owns the passthrough output.
                continue;
            } else if (project.getBuilderFromExtension(source) == null) {
                selectedTask = project.createTask(source, CopyResource.class);
            } else {
                selectedTask = project.createTask(source);
            }
            if (selectedTask != null && !selectedTask.getOutputs().isEmpty()) {
                selectedTasks.put(path, selectedTask);
            }
        }

        reuseInputTasks(project, existingTasks);
        // Project clears its task-creation map before execution. Retain the declarations, including generated tasks.
        resourceTasks = List.copyOf(project.getTasks());
    }

    /** Retains compiler flags for untouched resources during a selective build. */
    public Map<String, Set<Task.OutputFlags>> previousOutputFlags(Project project) {
        return previousGraph == null ? Map.of() : previousGraph.outputFlags(project);
    }

    /**
     * Reuses an existing owner when a selected input would create duplicate outputs, while preserving independently
     * needed tasks.
     */
    private void reuseInputTasks(Project project, Set<Task> existingTasks) {
        Map<IResource, List<Task>> consumers = new LinkedHashMap<>();
        Set<IResource> compiledInputs = new LinkedHashSet<>();
        for (Task task : project.getTasks()) {
            for (IResource input : task.getInputs()) {
                if (input.isOutput()) {
                    compiledInputs.add(input);
                } else if (!input.equals(task.firstInput())) {
                    consumers.computeIfAbsent(input, k -> new ArrayList<>()).add(task);
                }
            }
        }

        for (Map.Entry<String, Task> entry : selectedTasks.entrySet()) {
            Task task = entry.getValue();
            // A folder rule may select both an atlas and its input image. Reuse the
            // atlas's declared outputs, but keep independently needed runtime tasks.
            if (!existingTasks.contains(task) && task.getOutputs().stream().noneMatch(compiledInputs::contains)) {
                for (Task owner : consumers.getOrDefault(task.firstInput(), List.of())) {
                    if (owner.getOutputs().containsAll(task.getOutputs())) {
                        project.removeTask(task);
                        entry.setValue(owner);
                        break;
                    }
                }
            }
        }
    }

    /**
     * Converts a build output to its runtime archive path by removing the build-directory prefix and adding a leading
     * slash.
     */
    public static String outputPath(Project project, IResource resource) {
        String path = resource.getPath().replace('\\', '/');
        String build = project.getBuildDirectory().replace('\\', '/') + "/";
        return PackageResolver.normalize(path.startsWith(build) ? path.substring(build.length()) : path);
    }

    /**
     * Extends the compiled runtime graph with selected outputs and derives source mappings from retained task declarations.
     * Resolves manifest membership or preserves the legacy Exclude classification, then updates each graph node's base flag.
     *
     * @param bootstrapRoots runtime paths required to start the application
     * @param customResources opaque custom-resource outputs, included without traversing their contents
     * @param export whether Live Update content is emitted externally; false keeps selected content in the base
     * @return the plan, including diagnostics that the caller must validate before writing
     */
    public PackageResolver.Plan resolve(Project project, ResourceGraph graph, Collection<String> bootstrapRoots,
            Collection<IResource> customResources, boolean export) throws CompileExceptionError {
        Set<String> customRoots = new TreeSet<>();
        for (IResource resource : customResources) {
            String path = outputPath(project, resource);
            customRoots.add(path);
            // Custom resources are opaque, even if their extension happens to name a compiled type.
        }

        Map<String, List<String>> sourceOutputs = previousGraph == null ? new LinkedHashMap<>() : previousGraph.sources();
        Map<String, String> primarySources = previousGraph == null ? new LinkedHashMap<>() : previousGraph.primarySources();
        Set<String> refreshedSources = new TreeSet<>();
        Set<String> refreshedOutputs = new TreeSet<>();
        for (Task task : resourceTasks) {
            if (task.getBuilder() instanceof CopyCustomResourcesBuilder) {
                for (IResource input : task.getInputs()) {
                    refreshedSources.add(PackageResolver.normalize(input.getPath()));
                }
            } else if (!task.firstInput().isOutput()) {
                refreshedSources.add(PackageResolver.normalize(task.firstInput().getPath()));
            }
            for (IResource output : task.getOutputs()) {
                refreshedOutputs.add(outputPath(project, output));
            }
        }
        // Generated output names can change when a source is rebuilt. Replace its old mappings as well.
        for (Map.Entry<String, String> entry : primarySources.entrySet()) {
            if (refreshedSources.contains(entry.getValue())) {
                refreshedOutputs.add(entry.getKey());
            }
        }
        if (!manifestPath(project).isEmpty()) {
            for (Task task : selectedTasks.values()) {
                // Runtime references discover generated outputs, excluding build-only intermediates.
                if (!task.getOutputFlags(0).contains(Task.OutputFlags.BUILD_ONLY)) {
                    graph.add(project.getResource(outputPath(project, task.output(0))));
                }
            }
            if (previousGraph != null) {
                // Keep untouched compiled results available for other package selections and later rule edits.
                for (List<String> outputs : sourceOutputs.values()) {
                    for (String output : outputs) {
                        if (!refreshedOutputs.contains(output) && !customRoots.contains(output)) {
                            graph.add(project.getResource(output).disableMinifyPath());
                        }
                    }
                }
            }
        }
        Map<String, PackageResolver.Resource> resources = resources(graph, customRoots);
        sourceOutputs.values().forEach(outputs -> outputs.removeIf(refreshedOutputs::contains));
        primarySources.keySet().removeAll(refreshedOutputs);
        for (Task task : resourceTasks) {
            if (task.getBuilder().isGameProjectBuilder()) {
                continue;
            }
            for (IResource input : task.getInputs()) {
                if (!input.isOutput()) {
                    sourceOutputs.computeIfAbsent(PackageResolver.normalize(input.getPath()), key -> new ArrayList<>());
                }
            }
        }
        for (Task task : resourceTasks) {
            if (task.getBuilder().isGameProjectBuilder()) {
                continue;
            }
            if (task.getBuilder() instanceof CopyCustomResourcesBuilder) {
                for (int i = 0; i < task.getInputs().size(); ++i) {
                    String source = PackageResolver.normalize(task.input(i).getPath());
                    String output = outputPath(project, task.output(i));
                    if (resources.containsKey(output)) {
                        sourceOutputs.computeIfAbsent(source, k -> new ArrayList<>()).add(output);
                        primarySources.put(output, source);
                    }
                }
            } else {
                List<String> outputs = task.getOutputs().stream()
                        .map(output -> outputPath(project, output))
                        .filter(resources::containsKey)
                        .toList();
                IResource primary = task.firstInput();
                if (!primary.isOutput()) {
                    String source = PackageResolver.normalize(primary.getPath());
                    for (String output : outputs) {
                        primarySources.put(output, source);
                    }
                }
                for (IResource input : task.getInputs()) {
                    if (input.isOutput()) {
                        continue;
                    }
                    List<String> mapped = sourceOutputs.get(PackageResolver.normalize(input.getPath()));
                    if (mapped != null) {
                        mapped.addAll(outputs);
                    }
                }
            }
        }

        if (!manifestPath(project).isEmpty()) {
            sourceOutputs.keySet().retainAll(inventory);
        }
        return resolveMapped(project, graph, bootstrapRoots, customRoots, sourceOutputs, primarySources, export);
    }

    /** Resolves the current manifest using only the saved compiled graph and mappings. */
    public PackageResolver.Plan resolve(Project project, BuildGraph buildGraph, ResourceGraph graph) throws CompileExceptionError {
        Map<String, List<String>> sources = buildGraph.sources();
        if (!manifestPath(project).isEmpty()) {
            PackageResolver.Selection selection = PackageResolver.select(manifest, inventory);
            validate(selection.diagnostics());
            for (List<String> matches : selection.matches().values()) {
                for (String source : matches) {
                    if (!sources.containsKey(source)) {
                        throw new CompileExceptionError(manifestResource, 0,
                                "Resource has not been built: " + source + "; run build before bundle");
                    }
                }
            }
            sources.keySet().retainAll(inventory);
            // Retain the full authoring inventory for diagnostics and output/source collision checks.
            for (String source : inventory) {
                sources.putIfAbsent(source, List.of());
            }
        }
        return resolveMapped(project, graph, buildGraph.bootstrap(), new TreeSet<>(buildGraph.custom()),
                sources, buildGraph.primarySources(), splitPackages(project));
    }

    private static Map<String, PackageResolver.Resource> resources(ResourceGraph graph, Collection<String> customRoots) {
        Map<String, PackageResolver.Resource> resources = new LinkedHashMap<>();
        for (ResourceNode node : graph.getNodes()) {
            Map<String, PackageResolver.Loading> dependencies = new LinkedHashMap<>();
            for (ResourceNode child : node.getChildren()) {
                dependencies.put(child.getPath(), PackageResolver.Loading.valueOf(node.getLoading().name()));
            }
            resources.put(node.getPath(), new PackageResolver.Resource(dependencies));
        }
        for (String path : customRoots) {
            resources.putIfAbsent(path, new PackageResolver.Resource(Map.of()));
        }

        return resources;
    }

    private PackageResolver.Plan resolveMapped(Project project, ResourceGraph graph, Collection<String> bootstrapRoots,
            Set<String> customRoots, Map<String, List<String>> sourceOutputs, Map<String, String> primarySources,
            boolean export) throws CompileExceptionError {
        Map<String, PackageResolver.Resource> resources = resources(graph, customRoots);
        PackageResolver.Plan plan;
        if (manifestPath(project).isEmpty()) {
            // Use the final legacy classification, not rules or the manifest resolver's startup locks.
            // Any non-excluded route wins, including routes through ordinary proxies and factories.
            Set<String> excluded = export ? graph.createExcludedResourcesList() : Set.of();
            Set<String> base = new TreeSet<>(resources.keySet());
            base.removeAll(excluded);
            PackageDesc description = PackageDesc.newBuilder()
                    .setName("liveupdate")
                    .setContainerPreset("legacy")
                    .setFileCompressionPreset("legacy")
                    .build();
            PackageResolver.Package pkg = new PackageResolver.Package(description, excluded, Map.of());
            plan = new PackageResolver.Plan(Map.of("liveupdate", pkg), base, Map.of(), sourceOutputs, primarySources,
                    resources, List.of(), Map.of("liveupdate", Map.of()));
        } else {
            plan = PackageResolver.resolve(manifest, sourceOutputs, primarySources, resources,
                    bootstrapRoots, customRoots, export);
        }

        for (ResourceNode node : graph.getNodes()) {
            node.setInMainBundle(plan.base().contains(node.getPath()));
        }
        return plan;
    }

    /**
     * Throws a compile error attributed to the manifest for the first error diagnostic; warnings do not fail the
     * build.
     */
    public void validate(List<PackageResolver.Diagnostic> diagnostics) throws CompileExceptionError {
        for (PackageResolver.Diagnostic diagnostic : diagnostics) {
            if (diagnostic.severity() == PackageResolver.Severity.ERROR) {
                throw new CompileExceptionError(manifestResource, 0, diagnostic.message());
            }
        }
    }
}
