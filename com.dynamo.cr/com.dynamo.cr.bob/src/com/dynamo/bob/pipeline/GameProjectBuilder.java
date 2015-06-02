package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.reflect.Method;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyCustomResourcesBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.graphics.proto.Graphics.Cubemap;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;
import com.dynamo.gui.proto.Gui;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.proto.DdfExtensions;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.render.proto.Render.DisplayProfiles;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.dynamo.spine.proto.Spine.SpineScene;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.google.protobuf.DescriptorProtos.FieldOptions;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

/**
 * Game project and disk archive builder.
 * @author chmu
 *
 */
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
        extToMessageClass.put(".modelc", ModelDesc.class);
        extToMessageClass.put(".input_bindingc", InputBinding.class);
        extToMessageClass.put(".gamepadsc", GamepadMaps.class);
        extToMessageClass.put(".renderc", RenderPrototypeDesc.class);
        extToMessageClass.put(".particlefxc", ParticleFX.class);
        extToMessageClass.put(".spinemodelc", SpineModelDesc.class);
        extToMessageClass.put(".spinescenec", SpineScene.class);
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
        leafResourceTypes.add(".meshc");
    }


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        TaskBuilder<Void> builder = Task.<Void> newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(".projectc"));
        if (project.option("archive", "false").equals("true")) {
            builder.addOutput(input.changeExt(".darc"));
        }

        project.buildResource(input, CopyCustomResourcesBuilder.class);


        // Load texture profile message if supplied and enabled
        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null && project.option("texture-profiles", "false").equals("true")) {

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

    private File createArchive(Collection<String> resources) throws IOException {
        RandomAccessFile outFile = null;
        File tempArchiveFile = File.createTempFile("tmp", "darc");
        tempArchiveFile.deleteOnExit();
        outFile = new RandomAccessFile(tempArchiveFile, "rw");
        outFile.setLength(0);

        String root = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());
        ArchiveBuilder ab = new ArchiveBuilder(root);
        boolean doCompress = project.getProjectProperties().getBooleanValue("project", "compress_archive", true);

        for (String s : resources) {
            // 2:d argument is true to use compression.
            // We then try to compress all entries.
            // If the compressed/uncompressed ratio > 0.95 we do not compress
            // to save on load time...
            ab.add(s, doCompress);
        }

        ab.write(outFile);
        outFile.close();
        return tempArchiveFile;
    }

    private static void findResources(Project project, Message node, Collection<String> resources) throws CompileExceptionError {
        List<FieldDescriptor> fields = node.getDescriptorForType().getFields();

        for (FieldDescriptor fieldDescriptor : fields) {
            FieldOptions options = fieldDescriptor.getOptions();
            FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            Object value = node.getField(fieldDescriptor);
            if (value instanceof Message) {
                findResources(project, (Message) value, resources);

            } else if (value instanceof List) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) value;
                for (Object v : list) {
                    if (v instanceof Message) {
                        findResources(project, (Message) v, resources);
                    } else if (isResource && v instanceof String) {
                        findResources(project, project.getResource((String) v), resources);
                    }
                }

            } else if (isResource && value instanceof String) {
                findResources(project, project.getResource((String) value), resources);
            }
        }
    }

    private static void findResources(Project project, IResource resource, Collection<String> resources) throws CompileExceptionError {
        if (resource.getPath().equals("") || resource.getPath().startsWith("/builtins")) {
            return;
        }

        if (resources.contains(resource.output().getAbsPath())) {
            return;
        }
        resources.add(resource.output().getAbsPath());

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
                builder.mergeFrom(resource.output().getContent());
                Object message = builder.build();
                findResources(project, (Message) message, resources);

            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            throw new CompileExceptionError(resource, -1, "No mapping for " + ext);
        }
    }


    public static HashSet<String> findResources(Project project) throws CompileExceptionError {
        HashSet<String> resources = new HashSet<String>();

        if (project.option("keep-unused", "false").equals("true")) {

            // All outputs of the project should be considered resources
            for (String path : project.getOutputs()) {
                resources.add(path);
            }

        } else {

            // Root nodes to follow
            for (String[] pair : new String[][] { {"bootstrap", "main_collection"}, {"bootstrap", "render"}, {"input", "game_binding"}, {"display", "display_profiles"}}) {
                String path = project.getProjectProperties().getStringValue(pair[0], pair[1]);
                if (path != null) {
                    findResources(project, project.getResource(path), resources);
                }
            }

        }

        // Custom resources
        String[] custom_resources = project.getProjectProperties().getStringValue("project", "custom_resources", "").split(",");
        for (String s : custom_resources) {
            s = s.trim();
            if (s.length() > 0) {
                IResource r = project.getResource(s);
                resources.add(r.output().getAbsPath());
            }
        }

        return resources;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FileInputStream is = null;

        BobProjectProperties properties = new BobProjectProperties();
        IResource input = task.input(0);
        try {
            properties.load(new ByteArrayInputStream(input.getContent()));
        } catch (Exception e) {
            throw new CompileExceptionError(input, -1, "Failed to parse game.project", e);
        }

        try {
            if (project.option("archive", "false").equals("true")) {
                HashSet<String> resources = findResources(project);

                // Make sure we don't try to archive the .darc or .projectc
                resources.remove(task.output(0).getAbsPath());
                resources.remove(task.output(1).getAbsPath());

                File archiveFile = createArchive(resources);
                is = new FileInputStream(archiveFile);
                IResource arcOut = task.getOutputs().get(1);
                arcOut.setContent(is);
                archiveFile.delete();
            }

            IResource in = task.getInputs().get(0);
            IResource projOut = task.getOutputs().get(0);
            projOut.setContent(in.getContent());

        } finally {
            IOUtils.closeQuietly(is);
        }
    }
}

