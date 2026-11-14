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

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.liveupdate.PackageBuilder;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.bob.archive.liveupdate.PackageReport;
import com.dynamo.bob.archive.liveupdate.PackageArtifacts;
import com.dynamo.bob.archive.liveupdate.PackageWriter;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultResource;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.pipeline.graph.BuildGraph;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.DependencyMetadata;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompression;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;

import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigScene;
import com.dynamo.rig.proto.Rig.AnimationSet;

@BuilderParams(name = "GameProjectBuilder", inExts = ".project", outExt = "", paramsForSignature = {"variant", "platform", "architectures", "sound-stream-enabled"})
public class GameProjectBuilder extends Builder {

    // Root nodes to follow (default values from engine.cpp)
    private static final int ROOT_NODE_INPUT_GAMEPADS_INDEX = 4;
    private static final String DEFAULT_GAMEPADS = "/builtins/input/default.gamepadsc";
    private static final String DEFAULT_GAMEPAD_DATABASE = "/builtins/input/gamecontrollerdb.txt";
    private static final String EXT_GAMEPADS = ".gamepads";
    private static final String EXT_GAMEPADSC = ".gamepadsc";

    static final String[][] ROOT_NODES = new String[][] {
            {"bootstrap", "main_collection", "/logic/main.collectionc"},
            {"bootstrap", "render", "/builtins/render/default.renderc"},
            {"bootstrap", "debug_init_script", null},
            {"input", "game_binding", "/input/game.input_bindingc"},
            {"input", "gamepads", DEFAULT_GAMEPADS},
            {"display", "display_profiles", "/builtins/render/default.display_profilesc"}};

    private String[] gameProjectDependencies;
    private PackageBuilder packages;

    private static final Logger logger = Logger.getLogger(GameProjectBuilder.class.getName());

    private RandomAccessFile createRandomAccessFile(File handle) throws IOException {
        RandomAccessFile file = new RandomAccessFile(handle, "rw");
        file.setLength(0);
        return file;
    }

    private static boolean isPathSet(String path) {
        return path != null && path.trim().length() > 0;
    }

    private static String getGamepadsOutputPath(String gamepadsPath, String gamepadDbPath) {
        if (isPathSet(gamepadsPath)) {
            return ResourceUtil.replaceExt(gamepadsPath, EXT_GAMEPADS, EXT_GAMEPADSC);
        } else if (isPathSet(gamepadDbPath)) {
            return ResourceUtil.changeExt(gamepadDbPath, EXT_GAMEPADSC);
        } else {
            return "";
        }
    }

    private void addGamepadTask(TaskBuilder builder) throws CompileExceptionError {
        String gamepadsPath = project.getProjectProperties().getStringValue("input", "gamepads", DEFAULT_GAMEPADS);
        String gamepadDbPath = project.getProjectProperties().getStringValue("input", "gamepad_database", DEFAULT_GAMEPAD_DATABASE);

        IResource gamepads = null;
        if (isPathSet(gamepadsPath)) {
            gamepads = BuilderUtil.checkResource(project, builder.firstInput(), "input.gamepads", ResourceUtil.replaceExt(gamepadsPath, EXT_GAMEPADSC, EXT_GAMEPADS));
            gamepads.disableMinifyPath();
        }

        IResource gamepadDb = null;
        if (isPathSet(gamepadDbPath)) {
            gamepadDb = BuilderUtil.checkResource(project, builder.firstInput(), "input.gamepad_database", gamepadDbPath);
            gamepadDb.disableMinifyPath();
        }

        if (gamepads != null || gamepadDb != null) {
            project.createGamepadTask(gamepadDb, gamepads);
        }
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        gameProjectDependencies = new String[ROOT_NODES.length];
        int index = 0;
        for (String[] tuples : ROOT_NODES) {
            gameProjectDependencies[index] = project.getProjectProperties().getStringValue(tuples[0], tuples[1], tuples[2]);
            index++;
        }
        gameProjectDependencies[ROOT_NODE_INPUT_GAMEPADS_INDEX] = getGamepadsOutputPath(
                project.getProjectProperties().getStringValue("input", "gamepads", DEFAULT_GAMEPADS),
                project.getProjectProperties().getStringValue("input", "gamepad_database", DEFAULT_GAMEPAD_DATABASE));
        boolean nonStandardGameProjectFile = !project.getGameProjectResource().getAbsPath().equals(input.getAbsPath());
        if (nonStandardGameProjectFile) {
            throw new CompileExceptionError(input, -1, "Found non-standard game.project file: " + input.getPath());
        }

        // We currently don't have a file mapping with an input -> output for certain files
        // These should to be setup in the corresponding builder!
        ProtoBuilder.addMessageClass(".animationsetc", AnimationSet.class);
        ProtoBuilder.addMessageClass(".fontc", FontMap.class);
        ProtoBuilder.addMessageClass(".spc", ShaderDesc.class);
        ProtoBuilder.addMessageClass(".meshsetc", MeshSet.class);
        ProtoBuilder.addMessageClass(".rigscenec", RigScene.class);
        ProtoBuilder.addMessageClass(".skeletonc", Skeleton.class);
        ProtoBuilder.addMessageClass(".texturesetc", TextureSet.class);

        project.createPublisher();
        TaskBuilder builder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.disableMinifyPath().changeExt(".projectc").disableCache());

        for (IResource propertyFile : project.getPropertyFilesAsResources()) {
            builder.addInput(propertyFile);
        }
        String certificates = project.getProjectProperties().getStringValue("network", "ssl_certificates");
        if (isPathSet(certificates)) {
            builder.addInput(BuilderUtil.checkResource(project, input, "network.ssl_certificates", certificates));
            builder.addOutput(project.getResource(BundleHelper.SSL_CERTIFICATES_NAME).disableMinifyPath().output());
        }

        List<String> bootstrap = new ArrayList<>(Arrays.stream(gameProjectDependencies).filter(GameProjectBuilder::isPathSet).toList());
        if (CopyCustomResourcesBuilder.getDependencyMetadataInputResource(project) != null) {
            bootstrap.add(PackageResolver.normalize(DependencyMetadata.OUTPUT_PATH));
        }
        List<String> custom = getCustomResources(project).stream().map(resource -> PackageBuilder.outputPath(project, resource)).toList();
        packages = new PackageBuilder(project);
        boolean buildBase = packages.prepareBuild(project, builder, bootstrap, custom);
        if (buildBase) {
            project.createTask(input, CopyCustomResourcesBuilder.class);
        }
        for (index = 0; buildBase && index < gameProjectDependencies.length; index++) {
            String path = gameProjectDependencies[index];
            // initial values already have 'c' in the end
            if (path != null && path.length() > 0) {
                String field = "";
                if (index < ROOT_NODES.length) {
                    String[] tuples = ROOT_NODES[index];
                    field = String.format("%s.%s", tuples[0], tuples[1]);
                }

                if (field.equals("input.gamepads")) {
                    addGamepadTask(builder);
                } else {
                    path = path.substring(0, path.length() - 1);
                    IResource res = BuilderUtil.checkResource(project, builder.firstInput(), field, path);
                    res.disableMinifyPath();
                    project.createTask(res);
                }
            }
        }

        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles", "/builtins/graphics/default.texture_profiles");
        IResource textureProfiles = BuilderUtil.checkResource(project, builder.firstInput(), "graphics.texture_profiles", textureProfilesPath);
        textureProfiles.disableMinifyPath();
        project.createTask(textureProfiles);

        IResource publisherSettingsResorce = project.getPublisher().getPublisherSettingsResorce();
        if (publisherSettingsResorce != null) {
            builder.addInput(publisherSettingsResorce);
        }

        packages.createTasks(project);
        return builder.build();
    }

    private int getResourcePadding() throws CompileExceptionError {
        int resourcePadding = 4;
        String resourcePaddingStr = project.option("archive-resource-padding", null);
        if (resourcePaddingStr != null) {
            // It's already verified by bob, but we have to still parse it again
            try {
                resourcePadding = Integer.parseInt(resourcePaddingStr);
            } catch (Exception e) {
                throw new CompileExceptionError(String.format("Could not parse --archive-resource-padding='%s' into a valid integer", resourcePaddingStr), e);
            }
        }
        return resourcePadding;
    }

    private void createArchive(ArchiveBuilder archiveBuilder, Collection<IResource> resources, RandomAccessFile archiveIndex, RandomAccessFile archiveData, Set<String> excludedResources) throws IOException, CompileExceptionError {
        TimeProfiler.start("createArchive");
        logger.info("GameProjectBuilder.createArchive");
        long tstart = System.currentTimeMillis();

        boolean doCompress = packages.baseCompression(project).getFileCompression() == FileCompression.LZ4;
        archiveBuilder.setCompressionLevel(packages.baseCompression(project).getLz4Level());
        for (IResource resource : resources) {
            String path = resource.getAbsPath();
            Set<Task.OutputFlags> flags = project.getOutputFlags(path);
            boolean compress = (flags == null || !flags.contains(Task.OutputFlags.UNCOMPRESSED)) && doCompress;
            boolean encrypt = (flags != null && flags.contains(Task.OutputFlags.ENCRYPTED));

            archiveBuilder.add(path, compress, encrypt);
        }

        TimeProfiler.addData("resources", resources.size());
        TimeProfiler.addData("excludedResources", excludedResources.size());

        TimeProfiler.start("writeArchive");

        archiveBuilder.write(archiveIndex, archiveData, excludedResources);
        archiveIndex.close();
        archiveData.close();
        TimeProfiler.stop();

        long tend = System.currentTimeMillis();
        logger.info("GameProjectBuilder.createArchive took %f", (tend-tstart)/1000.0);
        TimeProfiler.stop();
    }

    private Set<IResource> getCustomResources(Project project) {
        Set<IResource> resources = new HashSet<>();
        for (String path : CopyCustomResourcesBuilder.getCustomResourcePaths(project)) {
            IResource r = project.getResource(path);
            resources.add(r.output());
        }
        if (CopyCustomResourcesBuilder.getDependencyMetadataInputResource(project) != null) {
            resources.add(CopyCustomResourcesBuilder.getDependencyMetadataOutputResource(project));
        }
        return resources;
    }

    private ResourceGraph createResourceGraph(Project project) throws CompileExceptionError {
        ResourceGraph graph = new ResourceGraph(project);

        for (String path : gameProjectDependencies) {
            if (path != null && path.length() > 0) {
                IResource resource = project.getResource(path);
                graph.add(resource);
            }
        }
        if (CopyCustomResourcesBuilder.getDependencyMetadataInputResource(project) != null) {
            graph.add(project.getResource(DependencyMetadata.OUTPUT_PATH));
        }
        return graph;
    }

    private ManifestBuilder createManifestBuilder(ResourceGraph resourceGraph) throws IOException {
        String projectIdentifier = project.getProjectProperties().getStringValue("project", "title", "<anonymous>");
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        String supportedEngineVersionsString = project.getPublisher().getSupportedVersions();

        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA256);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
        manifestBuilder.setProjectIdentifier(projectIdentifier);
        manifestBuilder.setBuildVariant(variant);
        manifestBuilder.setResourceGraph(resourceGraph);

        manifestBuilder.addSupportedEngineVersion(EngineVersion.version);
        if (supportedEngineVersionsString != null) {
            String[] supportedEngineVersions = supportedEngineVersionsString.split("\\s*,\\s*");
            for (String supportedEngineVersion : supportedEngineVersions) {
                manifestBuilder.addSupportedEngineVersion(supportedEngineVersion.trim());
            }
        }

        return manifestBuilder;
    }

    // Used to transform an input game.project properties map to a game.projectc representation.
    // Can be used for doing build time properties' conversion.
    static public void transformGameProjectFile(BobProjectProperties properties) {
        String gamepadsPath = properties.getStringValue("input", "gamepads", DEFAULT_GAMEPADS);
        String gamepadDbPath = properties.getStringValue("input", "gamepad_database", DEFAULT_GAMEPAD_DATABASE);
        String[] projectCustomResources = properties.getStringArrayValue("project", "custom_resources", new String[0]);
        String[] customResources = properties.getStringArrayValueMerged("project", "custom_resources", new String[0]);

        properties.removePrivateFields();

        // Map deprecated 'variable_dt' to new settings resulting in same runtime behavior
        Boolean variableDt = properties.getBooleanValue("display", "variable_dt");
        if (variableDt != null && variableDt) {
            System.err.println("\nWarning! Setting 'variable_dt' in 'game.project' is deprecated. Disabling 'Vsync' and setting 'Frame cap' to 0 for equivalent behavior.");
            properties.putBooleanValue("display", "vsync", false);
            properties.putIntValue("display", "update_frequency", 0);
        }

        // Convert project title to a string which may be used as a folder name and save in project.title_as_file_name
        String title = properties.getStringValue("project", "title", "Unnamed");
        String fileNameTitle = BundleHelper.projectNameToBinaryName(title);
        properties.putStringValue("project", "title_as_file_name", fileNameTitle);

        properties.putStringValue("input", "gamepads", getGamepadsOutputPath(gamepadsPath, gamepadDbPath));
        properties.putStringValue("input", "gamepad_database", null);
        if (!Arrays.equals(projectCustomResources, customResources)) {
            properties.putStringValue("project", "custom_resources", String.join(", ", customResources));
        }
    }

    private static void setOutputContentFromFile(IResource output, File sourceFile) throws IOException {
        // Archive temp files are fully written and closed before this point. For regular
        // filesystem outputs we can move them directly and avoid streaming large archives
        // through setContent(); the fallback stream owns its cleanup locally.
        if (output instanceof DefaultResource) {
            File destination = new File(output.getAbsPath());
            File parent = destination.getParentFile();
            if (parent != null) {
                parent.mkdirs();
            }
            Files.move(sourceFile.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING);
            return;
        }

        try (FileInputStream inputStream = new FileInputStream(sourceFile)) {
            output.setContent(inputStream);
        }
    }

    /** Restores flags for compiled outputs whose sources are outside this build's package selection. */
    public Map<String, Set<Task.OutputFlags>> previousOutputFlags() {
        return packages.previousOutputFlags(project);
    }

    /** Records runtime dependencies after the entire build succeeds, including when compiler tasks were cached. */
    public void saveBuildGraph() throws IOException, CompileExceptionError {
        ResourceGraph graph = createResourceGraph(project);
        List<String> bootstrap = graph.getRootNode().getChildren().stream().map(node -> node.getPath()).toList();
        Set<IResource> custom = getCustomResources(project);
        PackageResolver.Plan plan = packages.resolve(project, graph, bootstrap, custom, PackageBuilder.splitPackages(project));
        packages.validate(plan.diagnostics());
        project.setLiveUpdatePlan(plan);
        for (PackageResolver.Diagnostic diagnostic : plan.diagnostics()) {
            logger.warning("%s", diagnostic.message());
        }
        BuildGraph.capture(project, graph, plan, bootstrap,
                custom.stream().map(resource -> PackageBuilder.outputPath(project, resource)).toList(),
                packages.packageRoots()).save(project);
    }

    /** Packages prebuilt resources and returns whether the application bundle was selected. */
    public static boolean bundle(Project project, BuildGraph buildGraph) throws IOException, CompileExceptionError {
        GameProjectBuilder builder = new GameProjectBuilder();
        builder.setProject(project);
        builder.packages = new PackageBuilder(project);
        return builder.bundlePackages(buildGraph);
    }

    private boolean bundlePackages(BuildGraph buildGraph) throws IOException, CompileExceptionError {
        ResourceGraph graph = buildGraph.graph(project);
        Set<String> selection = packages.selectedPackages(project);
        boolean includeBase = selection.contains("base");
        PackageResolver.Plan plan = packages.resolve(project, buildGraph, graph);
        project.setLiveUpdatePlan(plan);
        packages.validate(plan.diagnostics());
        for (PackageResolver.Diagnostic diagnostic : plan.diagnostics()) {
            logger.warning("%s", diagnostic.message());
        }
        Set<IResource> resources = new HashSet<>(graph.getResources());
        for (String path : buildGraph.custom()) {
            resources.add(project.getResource(path).disableMinifyPath().output());
        }
        Set<String> emitted = plan.emittedResources();
        resources.removeIf(resource -> !emitted.contains(PackageBuilder.outputPath(project, resource)));
        Set<String> excluded = new HashSet<>(emitted);
        if (includeBase) {
            excluded.removeAll(plan.base());
        }
        String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());
        ManifestBuilder manifest = createManifestBuilder(graph);
        ArchiveBuilder archive = new ArchiveBuilder(root, manifest, getResourcePadding(), project);
        File indexFile = project.createTempFile("defold.index_", ".arci");
        File dataFile = project.createTempFile("defold.data_", ".arcd");
        List<IResource> outputs = List.of(".arci", ".arcd", ".dmanifest").stream()
                .map(extension -> project.getGameProjectResource().disableMinifyPath().changeExt(extension)).toList();
        boolean writePackages = selection.stream().anyMatch(name -> !name.equals("base"));
        try (PackageWriter writer = writePackages ? new PackageWriter(project, plan, packages.baseCompression(project)) : null) {
            archive.setPreparedResourceConsumer(writer);
            try (RandomAccessFile index = createRandomAccessFile(indexFile);
                 RandomAccessFile data = createRandomAccessFile(dataFile)) {
                createArchive(archive, resources, index, data, excluded);
            }
            project.setArchiveBuilder(archive);
            List<IResource> reserved = new ArrayList<>(outputs);
            reserved.add(BuildGraph.resource(project));
            reserved.add(project.getGameProjectResource().disableMinifyPath().changeExt(".projectc"));
            reserved.add(PackageArtifacts.resource(project));
            reserved.add(PackageReport.resource(project));
            PackageWriter.WriteResult written = writer == null
                    ? new PackageWriter.WriteResult(List.of(), Map.of(), Map.of())
                    : writer.write(manifest, reserved, selection);
            if (includeBase) {
                setOutputContentFromFile(outputs.get(0), indexFile);
                setOutputContentFromFile(outputs.get(1), dataFile);
                outputs.get(2).setContent(manifest.buildManifest(true));
                for (IResource output : outputs) {
                    project.getState().putSignature(output.getAbsPath(), new byte[0]);
                }
            }
            PackageArtifacts.resource(project).setContent(PackageArtifacts.serializeOutputs(project, written.outputs()));
            PackageReport.resource(project).setContent(PackageReport.create(project, archive, plan, written, selection));
            return includeBase;
        } finally {
            Files.deleteIfExists(indexFile.toPath());
            Files.deleteIfExists(dataFile.toPath());
        }
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource input = task.input(0);

        BobProjectProperties properties = Project.loadProperties(project, input, project.getPropertyFiles(), true);
        transformGameProjectFile(properties);
        task.getOutputs().get(0).setContent(properties.serialize().getBytes(StandardCharsets.UTF_8));
        if (task.getOutputs().size() > 1) {
            String certificates = project.getProjectProperties().getStringValue("network", "ssl_certificates");
            task.output(1).setContent(project.getResource(certificates).getContent());
        }
    }

    @Override
    public boolean isGameProjectBuilder() {
        return true;
    }
}
