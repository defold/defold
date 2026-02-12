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

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.VertexAttribute;
import com.dynamo.gamesys.proto.Camera.CameraDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.LightDesc;
import com.dynamo.gamesys.proto.Label.LabelDesc;
import com.dynamo.gamesys.proto.Physics.ConvexShape;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.dynamo.gamesys.proto.Sprite.SpriteTexture;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.gamesys.proto.Tile.TileGrid;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.render.proto.Render.DisplayProfiles;
import com.dynamo.render.proto.RenderTarget.RenderTargetDesc;

public class ProtoBuilders {

    private static String[][] textureSrcExts = {{".png", ".texturec"}, {".jpg", ".texturec"}, {".jpeg", ".texturec"}, {".tga", ".texturec"}, {".cubemap", ".texturec"}, {".render_target", ".render_targetc"}};
    private static String[][] renderResourceExts = {{".render_target", ".render_targetc"}, {".material", ".materialc"}, {".compute", ".computec"}};

    public static String getTextureSetExt(String str) throws Exception {
        Map<String, String> types = TextureUtil.getAtlasFileTypes();
        String suffix = "." + FilenameUtils.getExtension(str);
        String replacement = types.getOrDefault(suffix, null);
        if (replacement != null) {
            return replacement;
        }
        throw new Exception(String.format("Cannot find a texture suffix replacement for texture: %s\n", str));
    }

    private static String minifyAndReplaceAllExts(String str, String[][] extList) {
        String out = str;
        for (String[] ext : extList) {
            out = ResourceUtil.minifyPathAndReplaceExt(out, ext[0], ext[1]);
        }
        return out;
    }

    public static String replaceTextureName(String str) {
        return minifyAndReplaceAllExts(str, textureSrcExts);
    }

    public static String replaceTextureSetName(String str) throws Exception {
        String replacement = getTextureSetExt(str);
        String suffix = "." + FilenameUtils.getExtension(str);
        return ResourceUtil.minifyPathAndReplaceExt(str, suffix, replacement);
    }

    private static MaterialDesc.Builder getMaterialBuilderFromResource(IResource res) throws IOException {
        if (res.getPath().isEmpty()) {
            return null;
        }

        IResource materialBuildResource      = res.changeExt(".materialc");
        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        materialBuilder.mergeFrom(materialBuildResource.getContent());
        return materialBuilder;
    }

    // TODO: Should we move this to a build resource?
    static Set<String> materialAtlasCompatabilityCache = new HashSet<String>();

    private static void validateMaterialAtlasCompatability(Project project, IResource resource, String materialProjectPath, MaterialDesc.Builder materialBuilder, String textureSet) throws IOException, CompileExceptionError {
        if (materialProjectPath.isEmpty())
        {
            return;
        }

        if (textureSet.endsWith("atlas")) {
            String cachedValidationKey = materialProjectPath + textureSet;
            if (materialAtlasCompatabilityCache.contains(cachedValidationKey)) {
                return;
            }

            IResource textureSetResource      = project.getResource(textureSet);
            IResource textureSetBuildResource = textureSetResource.changeExt(".a.texturesetc");

            TextureSet.Builder textureSetBuilder = TextureSet.newBuilder();
            textureSetBuilder.mergeFrom(textureSetBuildResource.getContent());

            int textureSetPageCount  = textureSetBuilder.getPageCount();
            int materialMaxPageCount = materialBuilder.getMaxPageCount();

            boolean textureIsPaged = textureSetPageCount != 0;
            boolean materialIsPaged = materialMaxPageCount != 0;

            if (!textureIsPaged && !materialIsPaged) {
                return;
            }

            if (textureIsPaged && !materialIsPaged) {
                throw new CompileExceptionError(resource, 0, "The Material does not support paged Atlases, but the selected Image is paged");
            }
            else if (!textureIsPaged && materialIsPaged) {
                throw new CompileExceptionError(resource, 0, "The Material expects a paged Atlas, but the selected Image is not paged");
            }
            else if (textureSetPageCount > materialMaxPageCount) {
                throw new CompileExceptionError(resource, 0, "The Material's 'Max Page Count' is not sufficient for the number of pages in the selected Image");
            }

            materialAtlasCompatabilityCache.add(cachedValidationKey);
        }
    }

    @ProtoParams(srcClass = CollectionProxyDesc.class, messageClass = CollectionProxyDesc.class)
    @BuilderParams(name="CollectionProxy", inExts=".collectionproxy", outExt=".collectionproxyc")
    public static class CollectionProxyBuilder extends ProtoBuilder<CollectionProxyDesc.Builder> {
        @Override
        protected CollectionProxyDesc.Builder transform(Task task, IResource resource, CollectionProxyDesc.Builder messageBuilder) throws CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "collection", messageBuilder.getCollection());
            return messageBuilder.setCollection(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getCollection(), ".collection", ".collectionc"));
        }
    }

    @ProtoParams(srcClass = ConvexShape.class, messageClass = ConvexShape.class)
    @BuilderParams(name="ConvexShape", inExts=".convexshape", outExt=".convexshapec")
    public static class ConvexShapeBuilder extends ProtoBuilder<ConvexShape.Builder> {}

    @ProtoParams(srcClass = CameraDesc.class, messageClass = CameraDesc.class)
    @BuilderParams(name="Camera", inExts=".camera", outExt=".camerac")
    public static class CameraBuilder extends ProtoBuilder<CameraDesc.Builder> {}

    @ProtoParams(srcClass = InputBinding.class, messageClass = InputBinding.class)
    @BuilderParams(name="InputBinding", inExts=".input_binding", outExt=".input_bindingc")
    public static class InputBindingBuilder extends ProtoBuilder<InputBinding.Builder> {}

    @ProtoParams(srcClass = GamepadMaps.class, messageClass = GamepadMaps.class)
    @BuilderParams(name="GamepadMaps", inExts=".gamepads", outExt=".gamepadsc")
    public static class GamepadMapsBuilder extends ProtoBuilder<GamepadMaps.Builder> {
        @Override
        public Task create(IResource input) throws IOException, CompileExceptionError {
            Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                    .setName(params.name())
                    .addInput(input)
                    .addOutput(input.changeExt(params.outExt()));
            return taskBuilder.build();
        }
    }

    @ProtoParams(srcClass = RenderTargetDesc.class, messageClass = RenderTargetDesc.class)
    @BuilderParams(name="RenderTarget", inExts=".render_target", outExt=".render_targetc")
    public static class RenderTargetDescBuilder extends ProtoBuilder<RenderTargetDesc.Builder> {}

    @ProtoParams(srcClass = FactoryDesc.class, messageClass = FactoryDesc.class)
    @BuilderParams(name="Factory", inExts=".factory", outExt=".factoryc")
    public static class FactoryBuilder extends ProtoBuilder<FactoryDesc.Builder> {
        @Override
        protected FactoryDesc.Builder transform(Task task, IResource resource, FactoryDesc.Builder messageBuilder) throws IOException,
                CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "prototype", messageBuilder.getPrototype());
            return messageBuilder.setPrototype(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getPrototype(), ".go", ".goc"));
        }
    }

    @ProtoParams(srcClass = CollectionFactoryDesc.class, messageClass = CollectionFactoryDesc.class)
    @BuilderParams(name="CollectionFactory", inExts=".collectionfactory", outExt=".collectionfactoryc")
    public static class CollectionFactoryBuilder extends ProtoBuilder<CollectionFactoryDesc.Builder> {
        @Override
        protected CollectionFactoryDesc.Builder transform(Task task, IResource resource, CollectionFactoryDesc.Builder messageBuilder) throws IOException,
                CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "prototype", messageBuilder.getPrototype());
            return messageBuilder.setPrototype(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getPrototype(), ".collection", ".collectionc"));
        }
    }

    @ProtoParams(srcClass = LightDesc.class, messageClass = LightDesc.class)
    @BuilderParams(name="Light", inExts=".light", outExt=".lightc")
    public static class LightBuilder extends ProtoBuilder<LightDesc.Builder> {}

    @ProtoParams(srcClass = RenderPrototypeDesc.class, messageClass = RenderPrototypeDesc.class)
    @BuilderParams(name="Render", inExts=".render", outExt=".renderc")
    public static class RenderPrototypeBuilder extends ProtoBuilder<RenderPrototypeDesc.Builder> {

        private boolean hasDuplicateNames(List<RenderPrototypeDesc.RenderResourceDesc> resourceDesc) {
            Set<String> uniqueNames = new HashSet<>();
            for (RenderPrototypeDesc.RenderResourceDesc resource : resourceDesc) {
                String name = resource.getName();
                if (uniqueNames.contains(name)) {
                    return false;
                }
                uniqueNames.add(name);
            }
            return true;
        }

        @Override
        protected RenderPrototypeDesc.Builder transform(Task task, IResource resource, RenderPrototypeDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {

            String scriptPath = messageBuilder.getScript();
            String suffix = BuilderUtil.getSuffix(scriptPath);
            if (!suffix.isEmpty() && !suffix.equals("render_script"))
            {
                throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.BuilderUtil_WRONG_RESOURCE_TYPE,
                        new String[] { scriptPath, suffix, "render_script" } ));
            }

            BuilderUtil.checkResource(this.project, resource, "script", messageBuilder.getScript());
            messageBuilder.setScript(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getScript(), ".render_script", ".render_scriptc"));

            // Content migration, the material entry is deprecated in the render proto
            // we should use render resources entry now!
            List<RenderPrototypeDesc.RenderResourceDesc> newRenderResourceList = new ArrayList<RenderPrototypeDesc.RenderResourceDesc>();
            for (RenderPrototypeDesc.MaterialDesc m : messageBuilder.getMaterialsList()) {
                BuilderUtil.checkResource(this.project, resource, "material", m.getMaterial());

                RenderPrototypeDesc.RenderResourceDesc.Builder resBuilder = RenderPrototypeDesc.RenderResourceDesc.newBuilder();
                resBuilder.setName(m.getName());
                resBuilder.setPath(ResourceUtil.minifyPathAndReplaceExt(m.getMaterial(), ".material", ".materialc"));
                newRenderResourceList.add(resBuilder.build());
            }

            for (RenderPrototypeDesc.RenderResourceDesc resourceDesc : messageBuilder.getRenderResourcesList()) {
                BuilderUtil.checkResource(this.project, resource, "render resource", resourceDesc.getPath());
                newRenderResourceList.add(RenderPrototypeDesc.RenderResourceDesc.newBuilder()
                                     .mergeFrom(resourceDesc)
                                     .setPath(minifyAndReplaceAllExts(resourceDesc.getPath(), renderResourceExts))
                                     .build());
            }

            if (!hasDuplicateNames(newRenderResourceList)) {
                throw new CompileExceptionError(resource, 0, "The render resource list contain one or more entries with duplicate names.");
            }

            messageBuilder.clearMaterials();
            messageBuilder.clearRenderResources();
            messageBuilder.addAllRenderResources(newRenderResourceList);

            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = SpriteDesc.class, messageClass = SpriteDesc.class)
    @BuilderParams(name="SpriteDesc", inExts=".sprite", outExt=".spritec")
    public static class SpriteDescBuilder extends ProtoBuilder<SpriteDesc.Builder>
    {
        @Override
        protected SpriteDesc.Builder transform(Task task, IResource resource, SpriteDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {

            if (messageBuilder.hasTileSet()) {
                String texture = messageBuilder.getTileSet();

                SpriteTexture.Builder textureBuilder = SpriteTexture.newBuilder();
                textureBuilder.setTexture(texture);
                textureBuilder.setSampler("");
                messageBuilder.clearTextures();
                messageBuilder.addTextures(textureBuilder.build());
                messageBuilder.clearTileSet();
            }

            MaterialDesc.Builder materialBuilder = getMaterialBuilderFromResource(this.project.getResource(messageBuilder.getMaterial()));

            validateMaterialAtlasCompatability(this.project, resource, messageBuilder.getMaterial(), materialBuilder, messageBuilder.getTileSet());

            List<SpriteTexture> textures = new ArrayList<>();
            for (SpriteTexture texture : messageBuilder.getTexturesList()) {
                SpriteTexture.Builder textureBuilder = SpriteTexture.newBuilder();
                textureBuilder.mergeFrom(texture);

                BuilderUtil.checkResource(this.project, resource, "tile source", textureBuilder.getTexture());

                try {
                    textureBuilder.setTexture(replaceTextureSetName(textureBuilder.getTexture()));
                } catch (Exception e) {
                    throw new CompileExceptionError(resource, -1, e.getMessage(), e);
                }

                textures.add(textureBuilder.build());
            }
            messageBuilder.clearTextures();
            messageBuilder.addAllTextures(textures);

            messageBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getMaterial(), "material", "materialc"));

            if (materialBuilder != null) {
                List<VertexAttribute> materialAttributes       = materialBuilder.getAttributesList();
                List<VertexAttribute> spriteAttributeOverrides = new ArrayList<VertexAttribute>();

                for (int i=0; i < messageBuilder.getAttributesCount(); i++) {
                    VertexAttribute spriteAttribute = messageBuilder.getAttributes(i);
                    VertexAttribute materialAttribute = GraphicsUtil.getAttributeByName(materialAttributes, spriteAttribute.getName());

                    if (materialAttribute != null) {
                        spriteAttributeOverrides.add(GraphicsUtil.buildVertexAttribute(spriteAttribute, materialAttribute));
                    }
                }

                messageBuilder.clearAttributes();
                messageBuilder.addAllAttributes(spriteAttributeOverrides);
            }

            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = LabelDesc.class, messageClass = LabelDesc.class)
    @BuilderParams(name="LabelDesc", inExts=".label", outExt=".labelc")
    public static class LabelDescBuilder extends ProtoBuilder<LabelDesc.Builder> {
        @Override
        protected LabelDesc.Builder transform(Task task, IResource resource, LabelDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "material", messageBuilder.getMaterial());
            BuilderUtil.checkResource(this.project, resource, "font", messageBuilder.getFont());
            messageBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getMaterial(), "material", "materialc"));
            messageBuilder.setFont(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getFont(), "font", "fontc"));
            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = TileGrid.class, messageClass = TileGrid.class)
    @BuilderParams(name="TileGrid", inExts={".tilegrid", ".tilemap"}, outExt=".tilemapc")
    public static class TileGridBuilder extends ProtoBuilder<TileGrid.Builder> {
        @Override
        protected TileGrid.Builder transform(Task task, IResource resource, TileGrid.Builder messageBuilder) throws IOException,
                CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "tile source", messageBuilder.getTileSet());
            messageBuilder.setTileSet(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getTileSet(), "tileset", "t.texturesetc"));
            messageBuilder.setTileSet(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getTileSet(), "tilesource", "t.texturesetc"));
            messageBuilder.setTileSet(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getTileSet(), "atlas", "a.texturesetc"));
            messageBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getMaterial(), "material", "materialc"));
            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = ParticleFX.class, messageClass = ParticleFX.class)
    @BuilderParams(name="ParticleFX", inExts=".particlefx", outExt=".particlefxc")
    public static class ParticleFXBuilder extends ProtoBuilder<ParticleFX.Builder> {
        @Override
        protected ParticleFX.Builder transform(Task task, IResource resource, ParticleFX.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            int emitterCount = messageBuilder.getEmittersCount();
            // Move modifiers to all emitters, clear the list at the end
            List<Modifier> modifiers = messageBuilder.getModifiersList();
            for (int i = 0; i < emitterCount; ++i) {
                Emitter.Builder emitterBuilder = Emitter.newBuilder(messageBuilder.getEmitters(i));
                BuilderUtil.checkResource(this.project, resource, "tile source", emitterBuilder.getTileSource());
                BuilderUtil.checkResource(this.project, resource, "material", emitterBuilder.getMaterial());

                MaterialDesc.Builder materialBuilder = getMaterialBuilderFromResource(this.project.getResource(emitterBuilder.getMaterial()));

                validateMaterialAtlasCompatability(this.project, resource, emitterBuilder.getMaterial(), materialBuilder, emitterBuilder.getTileSource());

                emitterBuilder.setTileSource(ResourceUtil.minifyPathAndReplaceExt(emitterBuilder.getTileSource(), "tileset", "t.texturesetc"));
                emitterBuilder.setTileSource(ResourceUtil.minifyPathAndReplaceExt(emitterBuilder.getTileSource(), "tilesource", "t.texturesetc"));
                emitterBuilder.setTileSource(ResourceUtil.minifyPathAndReplaceExt(emitterBuilder.getTileSource(), "atlas", "a.texturesetc"));
                emitterBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(emitterBuilder.getMaterial(), "material", "materialc"));

                Point3d ep = MathUtil.ddfToVecmath(emitterBuilder.getPosition());
                Quat4d er = MathUtil.ddfToVecmath(emitterBuilder.getRotation(), "%s emitter: %s".formatted(resource, emitterBuilder.getId()));
                for (Modifier modifier : modifiers) {
                    Modifier.Builder mb = Modifier.newBuilder(modifier);
                    Point3d p = MathUtil.ddfToVecmath(modifier.getPosition());
                    Quat4d r = MathUtil.ddfToVecmath(modifier.getRotation(), "%s emitter: %s modifier: %s".formatted(resource, emitterBuilder.getId(), mb.getType().toString()));
                    MathUtil.invTransform(ep, er, p);
                    mb.setPosition(MathUtil.vecmathToDDF(p));
                    MathUtil.invTransform(er, r);
                    mb.setRotation(MathUtil.vecmathToDDF(r));
                    emitterBuilder.addModifiers(mb.build());
                }

                List<VertexAttribute> materialAttributes        = materialBuilder.getAttributesList();
                List<VertexAttribute> emitterAttributeOverrides = new ArrayList<VertexAttribute>();

                for (int j=0; j < emitterBuilder.getAttributesCount(); j++) {
                    VertexAttribute emitterAttribute  = emitterBuilder.getAttributes(j);
                    VertexAttribute materialAttribute = GraphicsUtil.getAttributeByName(materialAttributes, emitterAttribute.getName());

                    if (materialAttribute != null) {
                        emitterAttributeOverrides.add(GraphicsUtil.buildVertexAttribute(emitterAttribute, materialAttribute));
                    }
                }

                emitterBuilder.clearAttributes();
                emitterBuilder.addAllAttributes(emitterAttributeOverrides);

                messageBuilder.setEmitters(i, emitterBuilder.build());
            }
            messageBuilder.clearModifiers();
            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = SoundDesc.class, messageClass = SoundDesc.class)
    @BuilderParams(name="SoundDesc", inExts=".sound", outExt=".soundc")
    public static class SoundDescBuilder extends ProtoBuilder<SoundDesc.Builder> {
        @Override
        protected SoundDesc.Builder transform(Task task, IResource resource, SoundDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            BuilderUtil.checkResource(this.project, resource, "sound", messageBuilder.getSound());
            messageBuilder.setSound(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getSound(), "wav", "wavc"));
            messageBuilder.setSound(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getSound(), "ogg", "oggc"));
            messageBuilder.setSound(ResourceUtil.minifyPathAndReplaceExt(messageBuilder.getSound(), "opus", "opusc"));
            return messageBuilder;
        }
    }

    @ProtoParams(srcClass = DisplayProfiles.class, messageClass = DisplayProfiles.class)
    @BuilderParams(name="DisplayProfiles", inExts=".display_profiles", outExt=".display_profilesc")
    public static class DisplayProfilesBuilder extends ProtoBuilder<DisplayProfiles.Builder> {}


}
