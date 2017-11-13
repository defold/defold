package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.io.StringReader;
import java.io.BufferedReader;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.graphics.proto.Graphics.Cubemap;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;
import com.dynamo.gui.proto.Gui;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.label.proto.Label.LabelDesc;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.model.proto.ModelProto.Model;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.proto.DdfExtensions;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.DisplayProfiles;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.RigScene;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.google.protobuf.DescriptorProtos.FieldOptions;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

@BuilderParams(name = "GameProjectBuilder", inExts = ".project", outExt = "", createOrder = 1000)
public class GameProjectBuilder extends Builder<Void> {

    private static Map<String, Class<? extends GeneratedMessage>> extToMessageClass = new HashMap<String, Class<? extends GeneratedMessage>>();
    private static Set<String> leafResourceTypes = new HashSet<String>();

    static {
        extToMessageClass.put(".collectionc", CollectionDesc.class);
        extToMessageClass.put(".collectionproxyc", CollectionProxyDesc.class);
        extToMessageClass.put(".goc", PrototypeDesc.class);
        extToMessageClass.put(".texturesetc", TextureSet.class);
        extToMessageClass.put(".guic", Gui.SceneDesc.class);
        extToMessageClass.put(".scriptc", LuaModule.class);
        extToMessageClass.put(".gui_scriptc", LuaModule.class);
        extToMessageClass.put(".render_scriptc", LuaModule.class);
        extToMessageClass.put(".luac", LuaModule.class);
        extToMessageClass.put(".tilegridc", TileGrid.class);
        extToMessageClass.put(".collisionobjectc", CollisionObjectDesc.class);
        extToMessageClass.put(".spritec", SpriteDesc.class);
        extToMessageClass.put(".factoryc", FactoryDesc.class);
        extToMessageClass.put(".collectionfactoryc", CollectionFactoryDesc.class);
        extToMessageClass.put(".materialc", MaterialDesc.class);
        extToMessageClass.put(".fontc", FontMap.class);
        extToMessageClass.put(".soundc", SoundDesc.class);
        extToMessageClass.put(".labelc", LabelDesc.class);
        extToMessageClass.put(".modelc", Model.class);
        extToMessageClass.put(".input_bindingc", InputBinding.class);
        extToMessageClass.put(".gamepadsc", GamepadMaps.class);
        extToMessageClass.put(".renderc", RenderPrototypeDesc.class);
        extToMessageClass.put(".particlefxc", ParticleFX.class);
        extToMessageClass.put(".spinemodelc", SpineModelDesc.class);
        extToMessageClass.put(".rigscenec", RigScene.class);
        extToMessageClass.put(".skeletonc", Skeleton.class);
        extToMessageClass.put(".meshsetc", MeshSet.class);
        extToMessageClass.put(".animationsetc", MeshSet.class);
        extToMessageClass.put(".cubemapc", Cubemap.class);
        extToMessageClass.put(".camerac", CameraDesc.class);
        extToMessageClass.put(".lightc", LightDesc.class);
        extToMessageClass.put(".gamepadsc", GamepadMaps.class);
        extToMessageClass.put(".display_profilesc", DisplayProfiles.class);

        leafResourceTypes.add(".texturec");
        leafResourceTypes.add(".vpc");
        leafResourceTypes.add(".fpc");
        leafResourceTypes.add(".wavc");
        leafResourceTypes.add(".oggc");
    }

    private RandomAccessFile createRandomAccessFile(File handle) throws IOException {
        handle.deleteOnExit();
        RandomAccessFile file = new RandomAccessFile(handle, "rw");
        file.setLength(0);
        return file;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        TaskBuilder<Void> builder = Task.<Void> newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(".projectc"));
        if (project.option("archive", "false").equals("true")) {
            builder.addOutput(input.changeExt(".arci"));
            builder.addOutput(input.changeExt(".arcd"));
            builder.addOutput(input.changeExt(".dmanifest"));
            builder.addOutput(input.changeExt(".public.der"));
            for (IResource output : project.getPublisher().getOutputs(input)) {
                builder.addOutput(output);
            }
        }

        project.buildResource(input, CopyCustomResourcesBuilder.class);


        // Load texture profile message if supplied and enabled
        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {

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
        }

        for (Task<?> task : project.getTasks()) {
            for (IResource output : task.getOutputs()) {
                builder.addInput(output);
            }
        }

        return builder.build();
    }

    private void createArchive(Collection<ResourceEntry> resources, RandomAccessFile archiveIndex, RandomAccessFile archiveData, ManifestBuilder manifestBuilder, List<ResourceEntry> excludedResources) throws IOException, CompileExceptionError {
        String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());
        ArchiveBuilder archiveBuilder = new ArchiveBuilder(root, manifestBuilder);
        boolean doCompress = project.getProjectProperties().getBooleanValue("project", "compress_archive", true);
        HashMap<String, EnumSet<Project.OutputFlags>> outputs = project.getOutputs();

        for (ResourceEntry s : resources) {
            EnumSet<Project.OutputFlags> flags = outputs.get(s.resourceAbsPath);
            boolean compress = (flags != null && flags.contains(Project.OutputFlags.UNCOMPRESSED)) ? false : doCompress;
            archiveBuilder.add(s.resourceAbsPath, compress);
        }

        Path resourcePackDirectory = Files.createTempDirectory("defold.resourcepack_");
        archiveBuilder.write(archiveIndex, archiveData, resourcePackDirectory, excludedResources);
        manifestBuilder.setArchiveIdentifier(archiveBuilder.getArchiveIndexHash());
        archiveIndex.close();
        archiveData.close();

        // Populate the zip archive with the resource pack
        for (File fhandle : (new File(resourcePackDirectory.toAbsolutePath().toString())).listFiles()) {
            if (fhandle.isFile()) {
                project.getPublisher().AddEntry(fhandle.getName(), fhandle);
            }
        }

        project.getPublisher().Publish();
        File resourcePackDirectoryHandle = new File(resourcePackDirectory.toAbsolutePath().toString());
        if (resourcePackDirectoryHandle.exists() && resourcePackDirectoryHandle.isDirectory()) {
            FileUtils.deleteDirectory(resourcePackDirectoryHandle);
        }
    }

    private static void findResources(Project project, Message node, Collection<ResourceEntry> resources, ResourceNode parentNode) throws CompileExceptionError {
        List<FieldDescriptor> fields = node.getDescriptorForType().getFields();

        for (FieldDescriptor fieldDescriptor : fields) {
            FieldOptions options = fieldDescriptor.getOptions();
            FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            Object value = node.getField(fieldDescriptor);
            if (value instanceof Message) {
                findResources(project, (Message) value, resources, parentNode);
            } else if (value instanceof List) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) value;
                for (Object v : list) {
                    if (v instanceof Message) {
                        findResources(project, (Message) v, resources, parentNode);
                    } else if (isResource && v instanceof String) {
                        findResources(project, project.getResource((String) v), resources, parentNode);
                    }
                }
            } else if (isResource && value instanceof String) {
                findResources(project, project.getResource((String) value), resources, parentNode);
            }
        }
    }

    private static void findResources(Project project, IResource resource, Collection<ResourceEntry> resources, ResourceNode parentNode) throws CompileExceptionError {
        if (resource.getPath().equals("") ) {
            return;
        }

        if (resources.contains(new ResourceEntry(resource.output().getAbsPath(), parentNode.relativeFilepath))) {
            return;
        }

        resources.add(new ResourceEntry(resource.output().getAbsPath(), parentNode.relativeFilepath));

        ResourceNode currentNode = new ResourceNode(resource.getPath(), resource.output().getAbsPath());
        parentNode.addChild(currentNode);

        int i = resource.getPath().lastIndexOf(".");
        if (i == -1) {
            return;
        }
        String ext = resource.getPath().substring(i);

        if (leafResourceTypes.contains(ext)) {
            return;
        }

        Class<? extends GeneratedMessage> klass = extToMessageClass.get(ext);
        if (klass != null) {
            GeneratedMessage.Builder<?> builder;
            try {
                Method newBuilder = klass.getDeclaredMethod("newBuilder");
                builder = (GeneratedMessage.Builder<?>) newBuilder.invoke(null);
                final byte[] content = resource.output().getContent();
                if(content == null) {
                    throw new CompileExceptionError(resource, 0, "Unable to find resource " + resource.getPath());
                }
                builder.mergeFrom(content);
                Object message = builder.build();
                findResources(project, (Message) message, resources, currentNode);

            } catch(CompileExceptionError e) {
                throw e;
            } catch(Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            throw new CompileExceptionError(resource, -1, "No mapping for " + ext);
        }
    }


    public static HashSet<ResourceEntry> findResources(Project project, ResourceNode rootNode) throws CompileExceptionError {
        HashSet<ResourceEntry> resources = new HashSet<ResourceEntry>();

        if (project.option("keep-unused", "false").equals("true")) {

            // All outputs of the project should be considered resources
            for (String path : project.getOutputs().keySet()) {
                resources.add(new ResourceEntry(path, ""));
            }

        } else {

            // Root nodes to follow (default values from engine.cpp)
            for (String[] tuples : new String[][] { {"bootstrap", "main_collection", "/logic/main.collectionc"},
                                                    {"bootstrap", "render", "/builtins/render/default.renderc"},
                                                    {"input", "game_binding", "/input/game.input_bindingc"},
                                                    {"input", "gamepads", "/builtins/input/default.gamepadsc"},
                                                    {"display", "display_profiles", "/builtins/render/default.display_profilesc"}}) {
                String path = project.getProjectProperties().getStringValue(tuples[0], tuples[1], tuples[2]);
                if (path != null) {
                    findResources(project, project.getResource(path), resources, rootNode);
                }
            }

        }

        // Custom resources
        String[] custom_resources = project.getProjectProperties().getStringValue("project", "custom_resources", "").split(",");
        for (String s : custom_resources) {
            s = s.trim();
            if (s.length() > 0) {
                ArrayList<String> paths = new ArrayList<String>();
                project.findResourcePaths(s, paths);
                for (String path : paths) {
                    IResource r = project.getResource(path);
                    resources.add(new ResourceEntry(r.output().getAbsPath(), ""));
                }
            }
        }

        return resources;
    }

    private ManifestBuilder prepareManifestBuilder(ResourceNode rootNode, List<ResourceEntry> excludedResourcesList) throws IOException {
        String projectIdentifier = project.getProjectProperties().getStringValue("project", "title", "<anonymous>");
        String supportedEngineVersionsString = project.getProjectProperties().getStringValue("liveupdate", "supported_versions", null);
        String privateKeyFilepath = project.getProjectProperties().getStringValue("liveupdate", "privatekey", null);
        String publicKeyFilepath = project.getProjectProperties().getStringValue("liveupdate", "publickey", null);

        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setDependencies(rootNode);
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
        manifestBuilder.setProjectIdentifier(projectIdentifier);

        if (privateKeyFilepath == null || publicKeyFilepath == null) {
            File privateKeyFileHandle = File.createTempFile("defold.private_", ".der");
            privateKeyFileHandle.deleteOnExit();

            File publicKeyFileHandle = File.createTempFile("defold.public_", ".der");
            publicKeyFileHandle.deleteOnExit();

            privateKeyFilepath = privateKeyFileHandle.getAbsolutePath();
            publicKeyFilepath = publicKeyFileHandle.getAbsolutePath();
            try {
                ManifestBuilder.CryptographicOperations.generateKeyPair(SignAlgorithm.SIGN_RSA, privateKeyFilepath, publicKeyFilepath);
            } catch (NoSuchAlgorithmException exception) {
                throw new IOException("Unable to create manifest, cannot create asymmetric keypair!");
            }

        }
        manifestBuilder.setPrivateKeyFilepath(privateKeyFilepath);
        manifestBuilder.setPublicKeyFilepath(publicKeyFilepath);

        manifestBuilder.addSupportedEngineVersion(EngineVersion.sha1);
        if (supportedEngineVersionsString != null) {
            String[] supportedEngineVersions = supportedEngineVersionsString.split("\\s*,\\s*");
            for (String supportedEngineVersion : supportedEngineVersions) {
                manifestBuilder.addSupportedEngineVersion(supportedEngineVersion.trim());
            }
        }

        for (String excludedResource : project.getExcludedCollectionProxies()) {
            excludedResourcesList.add(new ResourceEntry(excludedResource, ""));
        }

        return manifestBuilder;
    }

    // Filter content of the game.project file.
    // Currently only strips away "project.dependencies" from the built file.
    static public byte[] filterProjectFileContent(IResource projectFile) throws IOException {
        BufferedReader bufReader = new BufferedReader(new StringReader(new String(projectFile.getContent())));

        String outputContent = "";
        String category = null;
        String line;
        while( (line = bufReader.readLine()) != null ) {

            // Keep track of current category name
            String lineTrimmed = line.trim();
            if (lineTrimmed.startsWith("[") && lineTrimmed.endsWith("]"))  {
                category = line.substring(1, line.length()-1);
            }

            // Filter out project.dependencies from build version of game.project
            if (!(category.equalsIgnoreCase("project") && line.startsWith("dependencies"))) {
                outputContent += line + "\n";
            }
        }

        return outputContent.getBytes();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FileInputStream archiveIndexInputStream = null;
        FileInputStream archiveDataInputStream = null;
        FileInputStream resourcePackInputStream = null;
        FileInputStream publicKeyInputStream = null;

        BobProjectProperties properties = new BobProjectProperties();
        IResource input = task.input(0);
        try {
            properties.load(new ByteArrayInputStream(input.getContent()));
        } catch (Exception e) {
            throw new CompileExceptionError(input, -1, "Failed to parse game.project", e);
        }

        try {
            if (project.option("archive", "false").equals("true")) {
                ResourceNode rootNode = new ResourceNode("<AnonymousRoot>", "<AnonymousRoot>");
                HashSet<ResourceEntry> resources = findResources(project, rootNode);
                List<ResourceEntry> excludedResources = new ArrayList<ResourceEntry>();
                ManifestBuilder manifestBuilder = this.prepareManifestBuilder(rootNode, excludedResources);

                // Make sure we don't try to archive the .arci, .arcd, .projectc, .dmanifest, .resourcepack.zip, .public.der
                for (IResource resource : task.getOutputs()) {
                    resources.remove(new ResourceEntry(resource.getAbsPath(), ""));
                }

                // Create output for the data archive
                File archiveIndexHandle = File.createTempFile("defold.index_", ".arci");
                RandomAccessFile archiveIndex = createRandomAccessFile(archiveIndexHandle);
                File archiveDataHandle = File.createTempFile("defold.data_", ".arcd");
                RandomAccessFile archiveData = createRandomAccessFile(archiveDataHandle);
                createArchive(resources, archiveIndex, archiveData, manifestBuilder, excludedResources);

                // Create manifest
                byte[] manifestFile = manifestBuilder.buildManifest();

                // Write outputs to the build system
                archiveIndexInputStream = new FileInputStream(archiveIndexHandle);
                task.getOutputs().get(1).setContent(archiveIndexInputStream);

                archiveDataInputStream = new FileInputStream(archiveDataHandle);
                task.getOutputs().get(2).setContent(archiveDataInputStream);

                task.getOutputs().get(3).setContent(manifestFile);

                publicKeyInputStream = new FileInputStream(manifestBuilder.getPublicKeyFilepath());
                task.getOutputs().get(4).setContent(publicKeyInputStream);

                List<InputStream> publisherOutputs = project.getPublisher().getOutputResults();
                for (int i = 0; i < publisherOutputs.size(); ++i) {
                    task.getOutputs().get(5 + i).setContent(publisherOutputs.get(i));
                    IOUtils.closeQuietly(publisherOutputs.get(i));
                }
            }

            task.getOutputs().get(0).setContent(filterProjectFileContent(task.getInputs().get(0)));
        } finally {
            IOUtils.closeQuietly(archiveIndexInputStream);
            IOUtils.closeQuietly(archiveDataInputStream);
            IOUtils.closeQuietly(resourcePackInputStream);
            IOUtils.closeQuietly(publicKeyInputStream);
        }
    }
}
