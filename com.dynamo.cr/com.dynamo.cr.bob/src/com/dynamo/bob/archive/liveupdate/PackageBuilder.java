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
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.util.ReportGenerator;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.PackageDesc;
import com.google.protobuf.TextFormat;

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

/** Adapts actual Bob tasks and compiled resource references to the pure resolver. */
public final class PackageBuilder {
    private final IResource manifestResource;
    private final ManifestDesc manifest;
    private final List<String> inventory;
    private final List<Task> resourceTasks;
    private final Map<String, Task> selectedTasks = new LinkedHashMap<>();

    public static String manifestPath(Project project) {
        return project.option("package-manifest", project.getProjectProperties().getStringValue("liveupdate", "manifest", "")).trim();
    }

    public static ManifestDesc parse(byte[] content) throws IOException {
        ManifestDesc.Builder builder = ManifestDesc.newBuilder();
        TextFormat.merge(new String(content, StandardCharsets.UTF_8), builder);
        return builder.build();
    }

    static Path outputDirectory(Project project, Path projectRoot) throws IOException, CompileExceptionError {
        if (manifestPath(project).isEmpty()) return legacyOutput(project).getParent();
        IResource resource = project.getResource(manifestPath(project));
        try {
            return PackageOutputPath.resolve(parse(resource.getContent()).getOutputDirectory(), projectRoot, project::getSystemEnv);
        } catch (IllegalArgumentException e) {
            throw new CompileExceptionError(resource, 0, e.getMessage(), e);
        }
    }

    /** Preserve legacy destinations, including generated names across incremental builds. */
    static Path legacyOutput(Project project) throws IOException {
        PublisherSettings settings = project.getPublisher().getPublisherSettings();
        Path projectRoot = Path.of(project.getRootDirectory()).toAbsolutePath().normalize();
        if (settings.getMode() == PublisherSettings.PublishMode.Amazon) {
            return projectRoot.resolve(project.getBuildDirectory()).resolve("liveupdate");
        }
        boolean zip = settings.getMode() != PublisherSettings.PublishMode.Folder;
        String directory = zip ? settings.getZipFilepath() : settings.getOutputDirectory();
        Path root = projectRoot.resolve(directory == null ? "" : directory).normalize();
        String name = zip ? settings.getZipFilename() : settings.getOutputFolderName();
        if (name == null || name.isEmpty()) {
            for (PackageWriter.Output previous : PackageArtifacts.readBuildOutputs(project)) {
                if (previous.directory() != zip && previous.path().getParent().equals(root)) return previous.path();
            }
            String platform = project.option("platform", "generic");
            name = zip ? "defold.resourcepack_" + platform + "_" + java.util.UUID.randomUUID() + ".zip"
                       : "defold_resources_" + platform + System.currentTimeMillis() / 1000;
        } else if (zip && !name.endsWith(".zip")) {
            name += ".zip";
        }
        return root.resolve(name).normalize();
    }

    public PackageBuilder(Project project, Task.TaskBuilder task) throws IOException, CompileExceptionError {
        if (manifestPath(project).isEmpty()) {
            manifestResource = project.getGameProjectResource();
            manifest = ManifestDesc.getDefaultInstance();
            resourceTasks = List.copyOf(project.getTasks());
            inventory = resourceTasks.stream().flatMap(t -> t.getInputs().stream()).filter(input -> !input.isOutput())
                    .map(input -> PackageResolver.normalize(input.getPath())).distinct().sorted().toList();
            task.addExtraCacheKey("legacy-packages-v2");
            task.addExtraCacheKey(PackageCache.VERSION);
            task.addExtraCacheKey(PackageCache.optionsSignature(project.getOptions()));
            return;
        }
        manifestResource = project.getResource(manifestPath(project));
        if (!manifestResource.exists()) throw new CompileExceptionError(manifestResource, 0, "Live Update manifest does not exist");
        try {
            manifest = parse(manifestResource.getContent());
        } catch (Exception e) {
            throw new CompileExceptionError(manifestResource, 0, "Invalid Live Update manifest: " + e.getMessage(), e);
        }
        Set<String> paths = new TreeSet<>();
        project.findResourcePaths("", paths);
        // Outputs from previous builds are not authoring resources, even when their destination is in the project.
        java.nio.file.Path buildDirectory = java.nio.file.Path.of(project.getRootDirectory(), project.getBuildDirectory()).toAbsolutePath().normalize();
        Set<Path> reportOutputs = ReportGenerator.outputPaths(project);
        paths.removeIf(path -> {
            Path absolute = Path.of(project.getResource(path).getAbsPath()).toAbsolutePath().normalize();
            return absolute.startsWith(buildDirectory) || reportOutputs.contains(absolute);
        });
        Set<String> generatedPaths = PackageArtifacts.generatedPaths(java.nio.file.Path.of(project.getRootDirectory()));
        paths.removeIf(path -> PackageArtifacts.isGeneratedPath(path, generatedPaths));
        for (PackageWriter.Output output : PackageArtifacts.readBuildOutputs(project)) {
            paths.removeIf(path -> java.nio.file.Path.of(project.getResource(path).getAbsPath()).normalize().startsWith(output.path()));
        }
        inventory = paths.stream().map(PackageResolver::normalize).toList();
        PackageResolver.Selection selection = PackageResolver.select(manifest, inventory);
        validate(selection.diagnostics());
        task.addInput(manifestResource);
        // Inventory is part of the signature: newly matching files invalidate even an otherwise unchanged build.
        task.addExtraCacheKey(String.join("\n", inventory));
        task.addExtraCacheKey(PackageCache.VERSION);
        task.addExtraCacheKey(PackageCache.optionsSignature(project.getOptions()));
        // Environment expansion can change placement without changing any project file.
        if (project.option("liveupdate", "false").equals("true")) {
            task.addExtraCacheKey(outputDirectory(project, Path.of(project.getRootDirectory())).toString());
        }
        Map<String, PackageProvider> providers = PackageProviders.discover(project.getProjectClassLoaderScanner());
        for (String providerId : manifest.getPackagesList().stream().map(pkg -> pkg.getProvider()).distinct().sorted().toList()) {
            PackageProvider provider = providers.get(providerId);
            if (provider != null) task.addExtraCacheKey(providerId + ":" + PackageCache.providerSignature(provider));
        }
        Set<String> selected = new TreeSet<>();
        selection.matches().values().forEach(selected::addAll);
        Set<String> custom = new LinkedHashSet<>();
        CopyCustomResourcesBuilder.getCustomResourcePaths(project).forEach(p -> custom.add(PackageResolver.normalize(p)));
        Set<Task> existingTasks = Set.copyOf(project.getTasks());
        for (String path : selected) {
            IResource source = project.getResource(path);
            if (source.equals(project.getGameProjectResource()) || path.endsWith(".lumanifest")) {
                throw new CompileExceptionError(manifestResource, 0, "Package rule selects build configuration: " + path);
            }
            Task selectedTask;
            if (custom.contains(path)) {
                // The existing custom-resource task owns the passthrough output.
                continue;
            } else if (project.getBuilderFromExtension(source) == null) {
                selectedTask = project.createTask(source, CopyPackageResource.class);
            } else {
                selectedTask = project.createTask(source);
            }
            if (selectedTask != null && !selectedTask.getOutputs().isEmpty()) {
                selectedTasks.put(path, selectedTask);
            }
        }
        reuseInputTasks(project, existingTasks);
        for (Task selectedTask : new LinkedHashSet<>(selectedTasks.values())) task.addInputsFromOutputs(selectedTask);
        // Project clears its task-creation map before execution. Retain the declarations, including generated tasks.
        resourceTasks = List.copyOf(project.getTasks());
    }

    private void reuseInputTasks(Project project, Set<Task> existingTasks) {
        Map<IResource, List<Task>> consumers = new LinkedHashMap<>();
        Set<IResource> compiledInputs = new LinkedHashSet<>();
        for (Task task : project.getTasks()) {
            for (IResource input : task.getInputs()) {
                if (input.isOutput()) compiledInputs.add(input);
                else if (!input.equals(task.firstInput())) consumers.computeIfAbsent(input, k -> new ArrayList<>()).add(task);
            }
        }
        selectedTasks.replaceAll((path, task) -> {
            // A folder rule may select both an atlas and its input image. Reuse the
            // atlas's declared outputs, but keep independently needed runtime tasks.
            if (!existingTasks.contains(task) && task.getOutputs().stream().noneMatch(compiledInputs::contains)) {
                for (Task owner : consumers.getOrDefault(task.firstInput(), List.of())) {
                    if (owner.getOutputs().containsAll(task.getOutputs())) {
                        project.removeTask(task);
                        return owner;
                    }
                }
            }
            return task;
        });
    }

    public static String outputPath(Project project, IResource resource) {
        String path = resource.getPath().replace('\\', '/');
        String build = project.getBuildDirectory().replace('\\', '/') + "/";
        return PackageResolver.normalize(path.startsWith(build) ? path.substring(build.length()) : path);
    }

    public PackageResolver.Plan resolve(Project project, ResourceGraph graph, Collection<String> bootstrapRoots,
                                        Collection<IResource> customResources, boolean export) throws CompileExceptionError {
        for (Task task : selectedTasks.values()) {
            // Primary output is declared by the task. Its runtime references discover secondary/generated
            // outputs; task-only intermediates (component counts, shader sources, etc.) never enter the graph.
            if (!task.getOutputFlags(0).contains(Task.OutputFlags.BUILD_ONLY)) {
                graph.add(project.getResource(outputPath(project, task.output(0))));
            }
        }
        Set<String> customRoots = new TreeSet<>();
        for (IResource resource : customResources) {
            String path = outputPath(project, resource);
            customRoots.add(path);
            // Custom resources are opaque, even if their extension happens to name a compiled type.
        }
        Map<String, PackageResolver.Resource> resources = new LinkedHashMap<>();
        for (ResourceNode node : graph.getNodes()) {
            Map<String, PackageResolver.Loading> dependencies = new LinkedHashMap<>();
            for (ResourceNode child : node.getChildren()) {
                dependencies.put(child.getPath(), PackageResolver.Loading.valueOf(node.getLoading().name()));
            }
            resources.put(node.getPath(), new PackageResolver.Resource(dependencies));
        }
        for (String path : customRoots) resources.putIfAbsent(path, new PackageResolver.Resource(Map.of()));
        Map<String, List<String>> sourceOutputs = new LinkedHashMap<>();
        Map<String, String> primarySources = new LinkedHashMap<>();
        inventory.forEach(path -> sourceOutputs.put(path, new ArrayList<>()));
        for (Task task : resourceTasks) {
            if (task.getBuilder().isGameProjectBuilder()) continue;
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
                List<String> outputs = task.getOutputs().stream().map(output -> outputPath(project, output))
                        .filter(resources::containsKey).toList();
                IResource primary = task.firstInput();
                if (!primary.isOutput()) {
                    String source = PackageResolver.normalize(primary.getPath());
                    outputs.forEach(output -> primarySources.put(output, source));
                }
                for (IResource input : task.getInputs()) {
                    if (input.isOutput()) continue;
                    List<String> mapped = sourceOutputs.get(PackageResolver.normalize(input.getPath()));
                    if (mapped != null) mapped.addAll(outputs);
                }
            }
        }
        PackageResolver.Plan plan;
        if (manifestPath(project).isEmpty()) {
            // Use the final legacy classification, not rules or the manifest resolver's startup locks.
            // Any non-excluded route wins, including routes through ordinary proxies and factories.
            Set<String> excluded = export ? graph.createExcludedResourcesList() : Set.of();
            Set<String> base = new TreeSet<>(resources.keySet());
            base.removeAll(excluded);
            String provider = project.getPublisher().getPublisherSettings().getMode() == PublisherSettings.PublishMode.Zip ? "zip" : "folder";
            PackageDesc description = PackageDesc.newBuilder().setName("liveupdate").setProvider(provider).build();
            PackageResolver.Package pkg = new PackageResolver.Package(description, excluded, Map.of());
            plan = new PackageResolver.Plan(Map.of("liveupdate", pkg), base, Map.of(), sourceOutputs, primarySources,
                    resources, List.of(), Map.of("liveupdate", Map.of()));
        } else {
            plan = PackageResolver.resolve(manifest, sourceOutputs, primarySources, resources, bootstrapRoots, customRoots, export);
        }
        for (ResourceNode node : graph.getNodes()) node.setInMainBundle(plan.base().contains(node.getPath()));
        return plan;
    }

    public void validate(List<PackageResolver.Diagnostic> diagnostics) throws CompileExceptionError {
        for (PackageResolver.Diagnostic diagnostic : diagnostics) {
            if (diagnostic.severity() == PackageResolver.Severity.ERROR) {
                throw new CompileExceptionError(manifestResource, 0, diagnostic.message());
            }
        }
    }
}
