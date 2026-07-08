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
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.apache.commons.io.FileUtils;
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
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultResource;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.DependencyMetadata;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;

import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigScene;
import com.dynamo.rig.proto.Rig.AnimationSet;

import static com.dynamo.bob.util.ComponentsCounter.isCompCounterStorage;

@BuilderParams(name = "GameProjectBuilder", inExts = ".project", outExt = "", paramsForSignature = {"liveupdate", "variant", "archive", "archive-resource-padding",
                "platform", "build-report-json", "build-report-html"})
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

    static String[] gameProjectDependencies;

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
            builder.addInputsFromOutputs(project.createGamepadTask(gamepadDb, gamepads));
        }
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        gameProjectDependencies = new String[ROOT_NODES.length + 1];
        int index = 0;
        for (String[] tuples : ROOT_NODES) {
            gameProjectDependencies[index] = project.getProjectProperties().getStringValue(tuples[0], tuples[1], tuples[2]);
            index++;
        }
        gameProjectDependencies[ROOT_NODE_INPUT_GAMEPADS_INDEX] = getGamepadsOutputPath(
                project.getProjectProperties().getStringValue("input", "gamepads", DEFAULT_GAMEPADS),
                project.getProjectProperties().getStringValue("input", "gamepad_database", DEFAULT_GAMEPAD_DATABASE));
        // Editor debugger scripts
        if (project.option("variant", Bob.VARIANT_RELEASE).equals(Bob.VARIANT_DEBUG)) {
            gameProjectDependencies[index] = "/builtins/scripts/debugger.luac";
            index++;
        }

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

        TimeProfiler.start("Add outputs");
        if (project.option("archive", "false").equals("true")) {
            builder.addOutput(input.disableMinifyPath().changeExt(".arci").disableCache());
            builder.addOutput(input.disableMinifyPath().changeExt(".arcd").disableCache());
            builder.addOutput(input.disableMinifyPath().changeExt(".dmanifest").disableCache());
            builder.addOutput(input.disableMinifyPath().changeExt(".graph.json").disableCache());
        }
        TimeProfiler.stop();

        createSubTask(input, CopyCustomResourcesBuilder.class, builder);
        for (index = 0; index < gameProjectDependencies.length; index++) {
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
                    createSubTask(res, builder);
                }
            }
        }

        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles", "/builtins/graphics/default.texture_profiles");
        IResource textureProfiles = BuilderUtil.checkResource(project, builder.firstInput(), "graphics.texture_profiles", textureProfilesPath);
        textureProfiles.disableMinifyPath();
        createSubTask(textureProfiles, builder);

        IResource publisherSettingsResorce = project.getPublisher().getPublisherSettingsResorce();
        if (publisherSettingsResorce != null) {
            builder.addInput(publisherSettingsResorce);
        }

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

    private void createArchive(ArchiveBuilder archiveBuilder, Collection<IResource> resources, RandomAccessFile archiveIndex, RandomAccessFile archiveData, List<String> excludedResources) throws IOException, CompileExceptionError {
        TimeProfiler.start("createArchive");
        logger.info("GameProjectBuilder.createArchive");
        long tstart = System.currentTimeMillis();

        boolean doCompress = project.getProjectProperties().getBooleanValue("project", "compress_archive", true);
        HashMap<String, EnumSet<Project.OutputFlags>> outputs = project.getOutputs();
        for (IResource resource : resources) {
            String path = resource.getAbsPath();
            EnumSet<Project.OutputFlags> flags = outputs.get(path);
            boolean compress = (flags == null || !flags.contains(Project.OutputFlags.UNCOMPRESSED)) && doCompress;
            boolean encrypt = (flags != null && flags.contains(Project.OutputFlags.ENCRYPTED));

            archiveBuilder.add(path, compress, encrypt);
        }

        TimeProfiler.addData("resources", resources.size());
        TimeProfiler.addData("excludedResources", excludedResources.size());

        TimeProfiler.start("writeArchive");

        Publisher publisher = project.getPublisher();
        if (publisher != null) publisher.start();
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

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource input = task.input(0);

        BobProjectProperties properties = Project.loadProperties(project, input, project.getPropertyFiles(), true);
        final String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());

        if (project.option("archive", "false").equals("true")) {
            // create the resource graphs
            // the full graph contains all resources in the project
            TimeProfiler.start("Generate resource graph");
            logger.info("Generating the resource graph");
            long tstart = System.currentTimeMillis();
            ResourceGraph resourceGraph = createResourceGraph(project);
            long tend = System.currentTimeMillis();
            logger.info("Generating the resource graph took %f s", (tend-tstart)/1000.0);
            TimeProfiler.stop();

            // create full list of resources including the custom resources
            // make sure to not archive the .arci, .arcd, .projectc, .dmanifest, or .resourcepack.zip
            // also make sure to not archive the comp counter files
            Set<IResource> resources = getCustomResources(project);
            resources.addAll(resourceGraph.getResources());
            for (IResource resource : task.getOutputs()) {
                resources.remove(resource);
            }

            TimeProfiler.start("Create excluded resources");
            logger.info("Creation of the excluded resources list.");
            tstart = System.currentTimeMillis();
            boolean shouldPublishLU = project.option("liveupdate", "false").equals("true");
            List<String> excludedResources;
            if (shouldPublishLU) {
                excludedResources = resourceGraph.createExcludedResourcesList();
            }
            else {
                excludedResources = new ArrayList<String>();
            }
            tend = System.currentTimeMillis();
            logger.info("Creation of the excluded resources list took %f s", (tend-tstart)/1000.0);
            TimeProfiler.stop();

            // Create output for the data archive
            String platform = project.option("platform", "generic");
            project.getPublisher().setPlatform(platform);
            File archiveIndexHandle = project.createTempFile("defold.index_", ".arci");
            File archiveDataHandle = project.createTempFile("defold.data_", ".arcd");

            // create the archive and manifest
            ManifestBuilder manifestBuilder = createManifestBuilder(resourceGraph);
            ArchiveBuilder archiveBuilder = new ArchiveBuilder(root, manifestBuilder, getResourcePadding(), project);
            try (RandomAccessFile archiveIndex = createRandomAccessFile(archiveIndexHandle);
                 RandomAccessFile archiveData = createRandomAccessFile(archiveDataHandle)) {
                createArchive(archiveBuilder, resources, archiveIndex, archiveData, excludedResources);
            }
            byte[] fullManifestFile = manifestBuilder.buildManifest();
            byte[] bundledManifestFile = manifestBuilder.buildManifest(true);
            this.project.setArchiveBuilder(archiveBuilder);

            // Write outputs to the build system
            // game.arci
            setOutputContentFromFile(task.getOutputs().get(1), archiveIndexHandle);

            // game.arcd
            setOutputContentFromFile(task.getOutputs().get(2), archiveDataHandle);

            // game.dmanifest
            task.getOutputs().get(3).setContent(bundledManifestFile);

            // game.graph.json
            resourceGraph.setHexDigests(archiveBuilder.getCachedHexDigests());
            logger.info("Writing the resource graph to json");
            tstart = System.currentTimeMillis();
            String resourceGraphJSON = resourceGraph.toJSON();
            task.getOutputs().get(4).setContent(resourceGraphJSON.getBytes());
            tend = System.currentTimeMillis();
            logger.info("Writing the resource graph to json took %f s", (tend-tstart)/1000.0);

            // Add copy of game.dmanifest to be published with liveupdate resources
            File manifestFileHandle = new File(task.getOutputs().get(3).getAbsPath());
            String liveupdateManifestFilename = "liveupdate.game.dmanifest";
            File manifestTmpFileHandle = new File(FilenameUtils.concat(manifestFileHandle.getParent(), liveupdateManifestFilename));
            FileUtils.writeByteArrayToFile(manifestTmpFileHandle, fullManifestFile);

            ArchiveEntry manifestArchiveEntry = new ArchiveEntry(root, manifestTmpFileHandle.getAbsolutePath());
            project.getPublisher().publish(manifestArchiveEntry, manifestTmpFileHandle);
            project.getPublisher().stop();

            // Copy SSL public keys if specified
            String sslCertificatesPath = project.getProjectProperties().getStringValue("network", "ssl_certificates");
            if (sslCertificatesPath != null && !sslCertificatesPath.isEmpty())
            {
                File source = new File(project.getRootDirectory(), sslCertificatesPath);
                File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
                File dist = new File(buildDir, BundleHelper.SSL_CERTIFICATES_NAME);
                FileUtils.copyFile(source, dist);
            }

            manifestTmpFileHandle.delete();
        }

        transformGameProjectFile(properties);
        task.getOutputs().get(0).setContent(properties.serialize().getBytes());
    }

    @Override
    public boolean isGameProjectBuilder() {
        return true;
    }
}
