// Copyright 2020-2024 The Defold Foundation
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
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Platform;
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
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.util.ComponentsCounter;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.MeshProto.MeshDesc;
import com.dynamo.gamesys.proto.ModelProto.Model;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.graphics.proto.Graphics.Cubemap;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigScene;
import com.dynamo.rig.proto.Rig.AnimationSet;

@BuilderParams(name = "GameProjectBuilder", inExts = ".project", outExt = "", createOrder = 1000)
public class GameProjectBuilder extends Builder<Void> {

    private static Logger logger = Logger.getLogger(GameProjectBuilder.class.getName());

    private RandomAccessFile createRandomAccessFile(File handle) throws IOException {
        FileUtil.deleteOnExit(handle);
        RandomAccessFile file = new RandomAccessFile(handle, "rw");
        file.setLength(0);
        return file;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        boolean nonStandardGameProjectFile = !project.getGameProjectResource().getAbsPath().equals(input.getAbsPath());
        if (nonStandardGameProjectFile) {
            throw new CompileExceptionError(input, -1, "Found non-standard game.project file: " + input.getPath());
        }

        // We currently don't have a file mapping with an input -> output for certain files
        // These should to be setup in the corresponding builder!
        ProtoBuilder.addMessageClass(".animationsetc", AnimationSet.class);
        ProtoBuilder.addMessageClass(".cubemapc", Cubemap.class);
        ProtoBuilder.addMessageClass(".fontc", FontMap.class);
        ProtoBuilder.addMessageClass(".fpc", ShaderDesc.class);
        ProtoBuilder.addMessageClass(".vpc", ShaderDesc.class);
        ProtoBuilder.addMessageClass(".goc", PrototypeDesc.class);
        ProtoBuilder.addMessageClass(".meshc", MeshDesc.class);
        ProtoBuilder.addMessageClass(".meshsetc", MeshSet.class);
        ProtoBuilder.addMessageClass(".modelc", Model.class);
        ProtoBuilder.addMessageClass(".rigscenec", RigScene.class);
        ProtoBuilder.addMessageClass(".skeletonc", Skeleton.class);
        ProtoBuilder.addMessageClass(".texturesetc", TextureSet.class);

        boolean shouldPublish = project.option("liveupdate", "false").equals("true");
        project.createPublisher(shouldPublish);
        TaskBuilder<Void> builder = Task.<Void> newBuilder(this)
                .setName(params.name())
                .disableCache()
                .addInput(input)
                .addOutput(input.changeExt(".projectc").disableCache());

        for (IResource propertyFile : project.getPropertyFilesAsResources()) {
            builder.addInput(propertyFile);
        }

        TimeProfiler.start("Add outputs");
        if (project.option("archive", "false").equals("true")) {
            builder.addOutput(input.changeExt(".arci").disableCache());
            builder.addOutput(input.changeExt(".arcd").disableCache());
            builder.addOutput(input.changeExt(".dmanifest").disableCache());
            builder.addOutput(input.changeExt(".public.der").disableCache());
            builder.addOutput(input.changeExt(".graph.json").disableCache());
            for (IResource output : project.getPublisher().getOutputs(input)) {
                builder.addOutput(output);
            }
        }
        TimeProfiler.stop();

        project.createTask(input, CopyCustomResourcesBuilder.class);

        // Load texture profile message if supplied and enabled
        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            TimeProfiler.start("Load texture profile");
            TextureProfiles.Builder texProfilesBuilder = TextureProfiles.newBuilder();
            IResource texProfilesInput = project.getResource(textureProfilesPath);
            if (!texProfilesInput.exists()) {
                throw new CompileExceptionError(input, -1, "Could not find supplied texture_profiles file: " + textureProfilesPath);
            }
            ProtoUtil.merge(texProfilesInput, texProfilesBuilder);

            // If Bob is building for a specific platform, we need to
            // filter out any platform entries not relevant to the target platform.
            // (i.e. we don't want win32 specific profiles lingering in android bundles)
            String targetPlatform = project.option("platform", "");

            List<TextureProfile> newProfiles = new LinkedList<TextureProfile>();
            for (int i = 0; i < texProfilesBuilder.getProfilesCount(); i++) {

                TextureProfile profile = texProfilesBuilder.getProfiles(i);
                TextureProfile.Builder profileBuilder = TextureProfile.newBuilder();
                profileBuilder.mergeFrom(profile);
                profileBuilder.clearPlatforms();

                // Take only the platforms that matches the target platform
                for (PlatformProfile platformProfile : profile.getPlatformsList()) {
                    if (Platform.matchPlatformAgainstOS(targetPlatform, platformProfile.getOs())) {
                        profileBuilder.addPlatforms(platformProfile);
                    }
                }

                newProfiles.add(profileBuilder.build());
            }

            // Update profiles list with new filtered one
            // Now it should only contain profiles with platform entries
            // relevant for the target platform...
            texProfilesBuilder.clearProfiles();
            texProfilesBuilder.addAllProfiles(newProfiles);


            // Add the current texture profiles to the project, since this
            // needs to be reachedable by the TextureGenerator.
            TextureProfiles textureProfiles = texProfilesBuilder.build();
            project.setTextureProfiles(textureProfiles);
            TimeProfiler.stop();
        }

        TimeProfiler.start("Add inputs");
        for (Task<?> task : project.getTasks()) {
            for (IResource output : task.getOutputs()) {
                builder.addInput(output);
            }
        }
        TimeProfiler.stop();

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

    private void createArchive(ArchiveBuilder archiveBuilder, Collection<IResource> resources, RandomAccessFile archiveIndex, RandomAccessFile archiveData, List<String> excludedResources, Path resourcePackDirectory) throws IOException, CompileExceptionError {
        TimeProfiler.start("createArchive");
        logger.info("GameProjectBuilder.createArchive");
        long tstart = System.currentTimeMillis();

        boolean doCompress = project.getProjectProperties().getBooleanValue("project", "compress_archive", true);
        HashMap<String, EnumSet<Project.OutputFlags>> outputs = project.getOutputs();
        for (IResource resource : resources) {
            String path = resource.getAbsPath();
            EnumSet<Project.OutputFlags> flags = outputs.get(path);
            boolean compress = (flags != null && flags.contains(Project.OutputFlags.UNCOMPRESSED)) ? false : doCompress;
            boolean encrypt = (flags != null && flags.contains(Project.OutputFlags.ENCRYPTED));
            archiveBuilder.add(path, compress, encrypt);
        }

        TimeProfiler.addData("resources", resources.size());
        TimeProfiler.addData("excludedResources", excludedResources.size());

        TimeProfiler.start("writeArchive");
        archiveBuilder.write(archiveIndex, archiveData, resourcePackDirectory, excludedResources);
        archiveIndex.close();
        archiveData.close();
        TimeProfiler.stop();

        // Populate publisher with the resource pack
        Publisher publisher = project.getPublisher();
        List<ArchiveEntry> excluded = archiveBuilder.getExcludedEntries();
        for (ArchiveEntry entry : excluded) {
            File f = new File(resourcePackDirectory.toAbsolutePath().toString(), entry.getHexDigest());
            publisher.AddEntry(f, entry);
        }

        long tend = System.currentTimeMillis();
        logger.info("GameProjectBuilder.createArchive took %f", (tend-tstart)/1000.0);
        TimeProfiler.stop();
    }

    private Set<IResource> getCustomResources(Project project) {
        Set<IResource> resources = new HashSet<>();
        String[] custom_resources = project.getProjectProperties().getStringValue("project", "custom_resources", "").split(",");
        for (String s : custom_resources) {
            s = s.trim();
            if (s.length() > 0) {
                ArrayList<String> paths = new ArrayList<String>();
                project.findResourcePaths(s, paths);
                for (String path : paths) {
                    IResource r = project.getResource(path);
                    resources.add(r.output());
                }
            }
        }
        return resources;
    }

    private ResourceGraph createResourceGraph(Project project) throws CompileExceptionError {
        ResourceGraph graph = new ResourceGraph(project);

        if (project.option("keep-unused", "false").equals("true")) {
            // All outputs of the project should be considered resources
            for (String path : project.getOutputs().keySet()) {
                // the paths are absolute and include the root directory
                // we need a path relative to the project root
                String relativePath = project.getPathRelativeToRootDirectory(path);
                IResource resource = project.getResource(relativePath);
                graph.add(resource);
            }
            return graph;
        }

        // Root nodes to follow (default values from engine.cpp)
        final String[][] ROOT_NODES = new String[][] {
            {"bootstrap", "main_collection", "/logic/main.collectionc"},
            {"bootstrap", "render", "/builtins/render/default.renderc"},
            {"bootstrap", "debug_init_script", null},
            {"input", "game_binding", "/input/game.input_bindingc"},
            {"input", "gamepads", "/builtins/input/default.gamepadsc"},
            {"display", "display_profiles", "/builtins/render/default.display_profilesc"}};
        for (String[] tuples : ROOT_NODES) {
            String path = project.getProjectProperties().getStringValue(tuples[0], tuples[1], tuples[2]);
            if (path != null) {
                IResource resource = project.getResource(path);
                graph.add(resource);
            }
        }

        // Editor debugger scripts
        if (project.option("variant", Bob.VARIANT_RELEASE).equals(Bob.VARIANT_DEBUG)) {
            IResource resource = project.getResource("/builtins/scripts/debugger.luac");
            graph.add(resource);
        }
        return graph;
    }

    private ManifestBuilder createManifestBuilder(ResourceGraph resourceGraph) throws IOException {
        String projectIdentifier = project.getProjectProperties().getStringValue("project", "title", "<anonymous>");
        String supportedEngineVersionsString = project.getPublisher().getSupportedVersions();
        String privateKeyFilepath = project.getPublisher().getManifestPrivateKey();
        String publicKeyFilepath = project.getPublisher().getManifestPublicKey();

        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA256);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
        manifestBuilder.setProjectIdentifier(projectIdentifier);
        manifestBuilder.setResourceGraph(resourceGraph);

        // Try manifest signing keys specified through the publisher
        if (!privateKeyFilepath.isEmpty() && !publicKeyFilepath.isEmpty() ) {
            if (!Files.exists(Paths.get(privateKeyFilepath))) {
                String altPrivateKeyFilepath = Paths.get(project.getRootDirectory(), privateKeyFilepath).toString();
                if (!Files.exists(Paths.get(altPrivateKeyFilepath))) {
                    throw new IOException(String.format("Couldn't find private key at either: '%s' or '%s'", privateKeyFilepath, altPrivateKeyFilepath));
                }
                privateKeyFilepath = altPrivateKeyFilepath;
            }

            if (!Files.exists(Paths.get(publicKeyFilepath))) {
                String altPublicKeyFilepath = Paths.get(project.getRootDirectory(), publicKeyFilepath).toString();
                if (!Files.exists(Paths.get(altPublicKeyFilepath))) {
                    throw new IOException(String.format("Couldn't find public key at either: '%s' or '%s'", publicKeyFilepath, altPublicKeyFilepath));
                }
                publicKeyFilepath = altPublicKeyFilepath;
            }
        }
        // Try manifest signing keys specified in project options
        else {
            privateKeyFilepath = project.option("manifest-private-key", "");
            if (!privateKeyFilepath.isEmpty() && !Files.exists(Paths.get(privateKeyFilepath))) {
                throw new IOException(String.format("Couldn't find private key at: '%s'", privateKeyFilepath));
            }
            publicKeyFilepath = project.option("manifest-public-key", "");
            if (!publicKeyFilepath.isEmpty() && !Files.exists(Paths.get(publicKeyFilepath))) {
                throw new IOException(String.format("Couldn't find public key at: '%s'", publicKeyFilepath));
            }
        }

        // If no keys were provided, generate them instead.
        if (privateKeyFilepath.isEmpty() || publicKeyFilepath.isEmpty()) {
            File privateKeyFileHandle = Paths.get(project.getRootDirectory(), "manifest.private.der").toFile();
            File publicKeyFileHandle = Paths.get(project.getRootDirectory(), "manifest.public.der").toFile();

            privateKeyFilepath = privateKeyFileHandle.getAbsolutePath();
            publicKeyFilepath = publicKeyFileHandle.getAbsolutePath();

            if (!privateKeyFileHandle.exists() || !publicKeyFileHandle.exists()) {
                logger.info("No public or private key for manifest signing set in liveupdate settings or project options, generating keys instead.");
                try {
                    ManifestBuilder.CryptographicOperations.generateKeyPair(SignAlgorithm.SIGN_RSA, privateKeyFilepath, publicKeyFilepath);
                } catch (NoSuchAlgorithmException exception) {
                    throw new IOException("Unable to create manifest, cannot create asymmetric keypair!");
                }
            }
        }

        manifestBuilder.setPrivateKeyFilepath(privateKeyFilepath);
        manifestBuilder.setPublicKeyFilepath(publicKeyFilepath);

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
    // Can be used for doing build time properties conversion.
    static public void transformGameProjectFile(BobProjectProperties properties) throws IOException {
        properties.removePrivateFields();

        // Map deprecated 'variable_dt' to new settings resulting in same runtime behavior
        Boolean variableDt = properties.getBooleanValue("display", "variable_dt");
        if (variableDt != null && variableDt == true) {
            System.err.println("\nWarning! Setting 'variable_dt' in 'game.project' is deprecated. Disabling 'Vsync' and setting 'Frame cap' to 0 for equivalent behavior.");
            properties.putBooleanValue("display", "vsync", false);
            properties.putIntValue("display", "update_frequency", 0);
        }

        // Convert project title to a string which may be used as a folder name and save in project.title_as_file_name
        String title = properties.getStringValue("project", "title", "Unnamed");
        String fileNameTitle = BundleHelper.projectNameToBinaryName(title);
        properties.putStringValue("project", "title_as_file_name", fileNameTitle);
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FileInputStream archiveIndexInputStream = null;
        FileInputStream archiveDataInputStream = null;
        FileInputStream resourcePackInputStream = null;
        FileInputStream publicKeyInputStream = null;

        IResource input = task.input(0);

        BobProjectProperties properties = Project.loadProperties(project, input, project.getPropertyFiles());
        final String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());

        try {
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
                // make sure to not archive the .arci, .arcd, .projectc, .dmanifest, .resourcepack.zip, .public.der
                // also make sure to not archive the comp counter files
                Set<IResource> resources = getCustomResources(project);
                resources.addAll(resourceGraph.getResources());
                for (IResource resource : task.getOutputs()) {
                    resources.remove(resource);
                }
                ComponentsCounter.excludeCounterPaths(resources);

                TimeProfiler.start("Create excluded resources");
                logger.info("Creation of the excluded resources list.");
                tstart = System.currentTimeMillis();
                boolean shouldPublishLU = project.option("liveupdate", "false").equals("true");
                List<String> excludedResources = null;
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
                File archiveIndexHandle = File.createTempFile("defold.index_", ".arci");
                RandomAccessFile archiveIndex = createRandomAccessFile(archiveIndexHandle);
                File archiveDataHandle = File.createTempFile("defold.data_", ".arcd");
                RandomAccessFile archiveData = createRandomAccessFile(archiveDataHandle);
                Path resourcePackDirectory = Files.createTempDirectory("defold.resourcepack_");

                // create the archive and manifest
                ManifestBuilder manifestBuilder = createManifestBuilder(resourceGraph);
                ArchiveBuilder archiveBuilder = new ArchiveBuilder(root, manifestBuilder, getResourcePadding());
                createArchive(archiveBuilder, resources, archiveIndex, archiveData, excludedResources, resourcePackDirectory);
                byte[] manifestFile = manifestBuilder.buildManifest();

                // Write outputs to the build system
                // game.arci
                archiveIndexInputStream = new FileInputStream(archiveIndexHandle);
                task.getOutputs().get(1).setContent(archiveIndexInputStream);

                // game.arcd
                archiveDataInputStream = new FileInputStream(archiveDataHandle);
                task.getOutputs().get(2).setContent(archiveDataInputStream);

                // game.dmanifest
                task.getOutputs().get(3).setContent(manifestFile);

                // game.public.der
                publicKeyInputStream = new FileInputStream(manifestBuilder.getPublicKeyFilepath());
                task.getOutputs().get(4).setContent(publicKeyInputStream);

                // game.graph.json
                resourceGraph.setHexDigests(archiveBuilder.getCachedHexDigests());
                logger.info("Writing the resource graph to json");
                tstart = System.currentTimeMillis();
                String resourceGraphJSON = resourceGraph.toJSON();
                task.getOutputs().get(5).setContent(resourceGraphJSON.getBytes());
                tend = System.currentTimeMillis();
                logger.info("Writing the resource graph to json took %f s", (tend-tstart)/1000.0);

                // Add copy of game.dmanifest to be published with liveupdate resources
                File manifestFileHandle = new File(task.getOutputs().get(3).getAbsPath());
                String liveupdateManifestFilename = "liveupdate.game.dmanifest";
                File manifestTmpFileHandle = new File(FilenameUtils.concat(manifestFileHandle.getParent(), liveupdateManifestFilename));
                FileUtils.copyFile(manifestFileHandle, manifestTmpFileHandle);

                ArchiveEntry manifestArchiveEntry = new ArchiveEntry(root, manifestTmpFileHandle.getAbsolutePath().toString());
                project.getPublisher().AddEntry(manifestTmpFileHandle, manifestArchiveEntry);
                project.getPublisher().Publish();

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
                File resourcePackDirectoryHandle = new File(resourcePackDirectory.toAbsolutePath().toString());
                if (resourcePackDirectoryHandle.exists() && resourcePackDirectoryHandle.isDirectory()) {
                    FileUtils.deleteDirectory(resourcePackDirectoryHandle);
                }

                List<InputStream> publisherOutputs = project.getPublisher().getOutputResults();
                for (int i = 0; i < publisherOutputs.size(); ++i) {
                    task.getOutputs().get(6 + i).setContent(publisherOutputs.get(i));
                    IOUtils.closeQuietly(publisherOutputs.get(i));
                }
            }

            transformGameProjectFile(properties);
            task.getOutputs().get(0).setContent(properties.serialize().getBytes());
        } finally {
            IOUtils.closeQuietly(archiveIndexInputStream);
            IOUtils.closeQuietly(archiveDataInputStream);
            IOUtils.closeQuietly(resourcePackInputStream);
            IOUtils.closeQuietly(publicKeyInputStream);
        }
    }
}
