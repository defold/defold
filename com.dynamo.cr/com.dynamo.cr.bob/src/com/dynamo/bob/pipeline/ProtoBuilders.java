package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionShape;
import com.dynamo.physics.proto.Physics.CollisionShape.Shape;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.tile.proto.Tile.TileGrid;

public class ProtoBuilders {

    private static String[] textureSrcExts = {".png", ".jpg", ".tga"};

    static String replaceTextureName(String str) {
        String out = str;
        for (String srcExt : textureSrcExts) {
            out = BuilderUtil.replaceExt(out, srcExt, ".texturec");
        }
        return out;
    }

    @ProtoParams(messageClass = CollectionDesc.class)
    @BuilderParams(name="Collection", inExts=".collection", outExt=".collectionc")
    public static class CollectionBuilder extends ProtoBuilder<CollectionDesc.Builder> {

        private void collectSubCollections(CollectionDesc collection, Set<IResource> subCollections) throws CompileExceptionError, IOException {
            for (CollectionInstanceDesc sub : collection.getCollectionInstancesList()) {
                IResource subResource = project.getResource(sub.getCollection());
                subCollections.add(subResource);
                CollectionDesc.Builder builder = CollectionDesc.newBuilder();
                ProtoUtil.merge(subResource, builder);
                collectSubCollections(builder.build(), subCollections);
            }
        }

        @Override
        public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
            Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                    .setName(params.name())
                    .addInput(input)
                    .addOutput(input.changeExt(params.outExt()));
            CollectionDesc.Builder builder = CollectionDesc.newBuilder();
            ProtoUtil.merge(input, builder);
            Set<IResource> subCollections = new HashSet<IResource>();
            collectSubCollections(builder.build(), subCollections);
            for (IResource subCollection : subCollections) {
                taskBuilder.addInput(subCollection);
            }
            return taskBuilder.build();
        }

        private void mergeSubCollections(CollectionDesc.Builder collectionBuilder) throws IOException, CompileExceptionError {
            Set<String> childIds = new HashSet<String>();
            for (CollectionInstanceDesc collInst : collectionBuilder.getCollectionInstancesList()) {
                IResource collResource = this.project.getResource(collInst.getCollection());
                CollectionDesc.Builder subCollBuilder = CollectionDesc.newBuilder();
                ProtoUtil.merge(collResource, subCollBuilder);
                mergeSubCollections(subCollBuilder);
                // Collect child ids
                childIds.clear();
                for (InstanceDesc inst : subCollBuilder.getInstancesList()) {
                    childIds.addAll(inst.getChildrenList());
                }
                Point3d p = MathUtil.ddfToVecmath(collInst.getPosition());
                Quat4d r = MathUtil.ddfToVecmath(collInst.getRotation());
                double s = collInst.getScale();
                String pathPrefix = collInst.getId() + "/";
                for (InstanceDesc inst : subCollBuilder.getInstancesList()) {
                    InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder(inst);
                    // merge id
                    instBuilder.setId(pathPrefix + inst.getId());
                    // merge transform for non-children
                    if (!childIds.contains(inst.getId())) {
                        Point3d instP = MathUtil.ddfToVecmath(inst.getPosition());
                        if (subCollBuilder.getScaleAlongZ() != 0) {
                            instP.scale(s);
                        } else {
                            instP.set(s * instP.getX(), s * instP.getY(), instP.getZ());
                        }
                        MathUtil.rotate(r, instP);
                        instP.add(p);
                        instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                        Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation());
                        instR.mul(r, instR);
                        instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                        instBuilder.setScale((float)(s * inst.getScale()));
                    }
                    // adjust child ids
                    for (int i = 0; i < instBuilder.getChildrenCount(); ++i) {
                        instBuilder.setChildren(i, pathPrefix + instBuilder.getChildren(i));
                    }
                    // add merged instance
                    collectionBuilder.addInstances(instBuilder);
                }
            }
            collectionBuilder.clearCollectionInstances();
        }

        @Override
        protected CollectionDesc.Builder transform(IResource resource, CollectionDesc.Builder messageBuilder) throws CompileExceptionError, IOException {

            mergeSubCollections(messageBuilder);

            for (int i = 0; i < messageBuilder.getInstancesCount(); ++i) {
                InstanceDesc.Builder b = InstanceDesc.newBuilder().mergeFrom(messageBuilder.getInstances(i));
                b.setId("/" + b.getId());
                for (int j = 0; j < b.getChildrenCount(); ++j) {
                    b.setChildren(j, "/" + b.getChildren(j));
                }
                BuilderUtil.checkFile(this.project, resource, "prototype", b.getPrototype());
                b.setPrototype(BuilderUtil.replaceExt(b.getPrototype(), ".go", ".goc"));
                for (int j = 0; j < b.getComponentPropertiesCount(); ++j) {
                    ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder(b.getComponentProperties(j));
                    PropertyDeclarations.Builder properties = PropertyDeclarations.newBuilder();
                    for (PropertyDesc desc : compPropBuilder.getPropertiesList()) {
                        if (!PropertiesUtil.transformPropertyDesc(resource, desc, properties)) {
                            throw new CompileExceptionError(resource, 0, String.format("The property %s.%s.%s has an invalid format: %s", b.getId(), compPropBuilder.getId(), desc.getId(), desc.getValue()));
                        }
                    }
                    compPropBuilder.setPropertyDecls(properties);
                    b.setComponentProperties(j, compPropBuilder);
                }
                messageBuilder.setInstances(i, b);
            }

            return messageBuilder;
        }


    }

    @ProtoParams(messageClass = CollectionProxyDesc.class)
    @BuilderParams(name="CollectionProxy", inExts=".collectionproxy", outExt=".collectionproxyc")
    public static class CollectionProxyBuilder extends ProtoBuilder<CollectionProxyDesc.Builder> {
        @Override
        protected CollectionProxyDesc.Builder transform(IResource resource, CollectionProxyDesc.Builder messageBuilder) throws CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "collection", messageBuilder.getCollection());
            return messageBuilder.setCollection(BuilderUtil.replaceExt(messageBuilder.getCollection(), ".collection", ".collectionc"));
        }
    }

    @ProtoParams(messageClass = ModelDesc.class)
    @BuilderParams(name="Model", inExts=".model", outExt=".modelc")
    public static class ModelBuilder extends ProtoBuilder<ModelDesc.Builder> {
        @Override
        protected ModelDesc.Builder transform(IResource resource, ModelDesc.Builder messageBuilder) throws CompileExceptionError {

            BuilderUtil.checkFile(this.project, resource, "mesh", messageBuilder.getMesh());
            messageBuilder.setMesh(BuilderUtil.replaceExt(messageBuilder.getMesh(), ".dae", ".meshc"));
            BuilderUtil.checkFile(this.project, resource, "material", messageBuilder.getMaterial());
            messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), ".material", ".materialc"));
            List<String> newTextureList = new ArrayList<String>();
            for (String t : messageBuilder.getTexturesList()) {
                newTextureList.add(replaceTextureName(t));
            }
            messageBuilder.clearTextures();
            messageBuilder.addAllTextures(newTextureList);
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = ConvexShape.class)
    @BuilderParams(name="ConvexShape", inExts=".convexshape", outExt=".convexshapec")
    public static class ConvexShapeBuilder extends ProtoBuilder<ConvexShape.Builder> {}


    @ProtoParams(messageClass = CollisionObjectDesc.class)
    @BuilderParams(name="CollisionObjectDesc", inExts=".collisionobject", outExt=".collisionobjectc")
    public static class CollisionObjectBuilder extends ProtoBuilder<CollisionObjectDesc.Builder> {

        @Override
        protected CollisionObjectDesc.Builder transform(IResource resource, CollisionObjectDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
            if (messageBuilder.getEmbeddedCollisionShape().getShapesCount() == 0) {
                BuilderUtil.checkFile(this.project, resource, "collision shape", messageBuilder.getCollisionShape());
            }
            // Merge convex shape resource with collision object
            // NOTE: Special case for tilegrid resources. They are left as is
            if (messageBuilder.hasCollisionShape() && !messageBuilder.getCollisionShape().isEmpty() && !(messageBuilder.getCollisionShape().endsWith(".tilegrid") || messageBuilder.getCollisionShape().endsWith(".tilemap"))) {
                IResource shapeResource = project.getResource(messageBuilder.getCollisionShape().substring(1));
                ConvexShape.Builder cb = ConvexShape.newBuilder();
                ProtoUtil.merge(shapeResource, cb);
                CollisionShape.Builder eb = CollisionShape.newBuilder().mergeFrom(messageBuilder.getEmbeddedCollisionShape());
                Shape.Builder sb = Shape.newBuilder()
                        .setShapeType(CollisionShape.Type.valueOf(cb.getShapeType().getNumber()))
                        .setPosition(Point3.newBuilder())
                        .setRotation(Quat.newBuilder().setW(1))
                        .setIndex(eb.getDataCount())
                        .setCount(cb.getDataCount());
                eb.addShapes(sb);
                eb.addAllData(cb.getDataList());
                messageBuilder.setEmbeddedCollisionShape(eb);
                messageBuilder.setCollisionShape("");
            }

            messageBuilder.setCollisionShape(BuilderUtil.replaceExt(messageBuilder.getCollisionShape(), ".convexshape", ".convexshapec"));
            messageBuilder.setCollisionShape(BuilderUtil.replaceExt(messageBuilder.getCollisionShape(), ".tilegrid", ".tilegridc"));
            messageBuilder.setCollisionShape(BuilderUtil.replaceExt(messageBuilder.getCollisionShape(), ".tilemap", ".tilegridc"));
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = SceneDesc.class)
    @BuilderParams(name="Gui", inExts=".gui", outExt=".guic")
    public static class GuiBuilder extends ProtoBuilder<SceneDesc.Builder> {
        @Override
        protected SceneDesc.Builder transform(IResource input, SceneDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
            messageBuilder.setScript(BuilderUtil.replaceExt(messageBuilder.getScript(), ".gui_script", ".gui_scriptc"));
            Set<String> fontNames = new HashSet<String>();
            Set<String> textureNames = new HashSet<String>();

            List<FontDesc> newFontList = new ArrayList<FontDesc>();
            for (FontDesc f : messageBuilder.getFontsList()) {
                fontNames.add(f.getName());
                newFontList.add(FontDesc.newBuilder().mergeFrom(f).setFont(BuilderUtil.replaceExt(f.getFont(), ".font", ".fontc")).build());
            }
            messageBuilder.clearFonts();
            messageBuilder.addAllFonts(newFontList);

            List<TextureDesc> newTextureList = new ArrayList<TextureDesc>();
            for (TextureDesc f : messageBuilder.getTexturesList()) {
                textureNames.add(f.getName());
                newTextureList.add(TextureDesc.newBuilder().mergeFrom(f).setTexture(replaceTextureName(f.getTexture())).build());
            }
            messageBuilder.clearTextures();
            messageBuilder.addAllTextures(newTextureList);

            for (NodeDesc n : messageBuilder.getNodesList()) {
                if (n.hasTexture() && n.getTexture().length() > 0) {
                    if (!textureNames.contains(n.getTexture())) {
                        throw new CompileExceptionError(input, 0, BobNLS.bind(Messages.GuiBuilder_MISSING_TEXTURE, n.getTexture()));
                    }
                }

                if (n.hasFont() && n.getFont().length() > 0) {
                    if (!fontNames.contains(n.getFont())) {
                        throw new CompileExceptionError(input, 0, BobNLS.bind(Messages.GuiBuilder_MISSING_FONT, n.getFont()));
                    }
                }

            }

            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = CameraDesc.class)
    @BuilderParams(name="Camera", inExts=".camera", outExt=".camerac")
    public static class CameraBuilder extends ProtoBuilder<CameraDesc.Builder> {}

    @ProtoParams(messageClass = InputBinding.class)
    @BuilderParams(name="InputBinding", inExts=".input_binding", outExt=".input_bindingc")
    public static class InputBindingBuilder extends ProtoBuilder<InputBinding.Builder> {}

    @ProtoParams(messageClass = GamepadMaps.class)
    @BuilderParams(name="GamepadMaps", inExts=".gamepads", outExt=".gamepadsc")
    public static class GamepadMapsBuilder extends ProtoBuilder<GamepadMaps.Builder> {}

    @ProtoParams(messageClass = FactoryDesc.class)
    @BuilderParams(name="Factory", inExts=".factory", outExt=".factoryc")
    public static class FactoryBuilder extends ProtoBuilder<FactoryDesc.Builder> {
        @Override
        protected FactoryDesc.Builder transform(IResource resource, FactoryDesc.Builder messageBuilder) throws IOException,
                CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "prototype", messageBuilder.getPrototype());
            return messageBuilder.setPrototype(BuilderUtil.replaceExt(messageBuilder.getPrototype(), ".go", ".goc"));
        }
    }

    @ProtoParams(messageClass = LightDesc.class)
    @BuilderParams(name="Light", inExts=".light", outExt=".lightc")
    public static class LightBuilder extends ProtoBuilder<LightDesc.Builder> {}

    @ProtoParams(messageClass = RenderPrototypeDesc.class)
    @BuilderParams(name="Render", inExts=".render", outExt=".renderc")
    public static class RenderPrototypeBuilder extends ProtoBuilder<RenderPrototypeDesc.Builder> {
        @Override
        protected RenderPrototypeDesc.Builder transform(IResource resource, RenderPrototypeDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {

            BuilderUtil.checkFile(this.project, resource, "script", messageBuilder.getScript());
            messageBuilder.setScript(BuilderUtil.replaceExt(messageBuilder.getScript(), ".render_script", ".render_scriptc"));

            List<RenderPrototypeDesc.MaterialDesc> newMaterialList = new ArrayList<RenderPrototypeDesc.MaterialDesc>();
            for (RenderPrototypeDesc.MaterialDesc m : messageBuilder.getMaterialsList()) {
                BuilderUtil.checkFile(this.project, resource, "material", m.getMaterial());
                newMaterialList.add(RenderPrototypeDesc.MaterialDesc.newBuilder().mergeFrom(m).setMaterial(BuilderUtil.replaceExt(m.getMaterial(), ".material", ".materialc")).build());
            }
            messageBuilder.clearMaterials();
            messageBuilder.addAllMaterials(newMaterialList);

            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = SpriteDesc.class)
    @BuilderParams(name="SpriteDesc", inExts=".sprite", outExt=".spritec")
    public static class SpriteDescBuilder extends ProtoBuilder<SpriteDesc.Builder> {
        @Override
        protected SpriteDesc.Builder transform(IResource resource, SpriteDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "tile source", messageBuilder.getTileSet());
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tileset", "texturesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tilesource", "texturesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "atlas", "texturesetc"));
            messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), "material", "materialc"));
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = TileGrid.class)
    @BuilderParams(name="TileGrid", inExts={".tilegrid", ".tilemap"}, outExt=".tilegridc")
    public static class TileGridBuilder extends ProtoBuilder<TileGrid.Builder> {
        @Override
        protected TileGrid.Builder transform(IResource resource, TileGrid.Builder messageBuilder) throws IOException,
                CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "tile source", messageBuilder.getTileSet());
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tileset", "texturesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tilesource", "texturesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "atlas", "texturesetc"));
            messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), "material", "materialc"));
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = ParticleFX.class)
    @BuilderParams(name="ParticleFX", inExts=".particlefx", outExt=".particlefxc")
    public static class ParticleFXBuilder extends ProtoBuilder<ParticleFX.Builder> {
        @Override
        protected ParticleFX.Builder transform(IResource resource, ParticleFX.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            int emitterCount = messageBuilder.getEmittersCount();
            // Move modifiers to all emitters, clear the list at the end
            List<Modifier> modifiers = messageBuilder.getModifiersList();
            for (int i = 0; i < emitterCount; ++i) {
                Emitter.Builder emitterBuilder = Emitter.newBuilder(messageBuilder.getEmitters(i));
                BuilderUtil.checkFile(this.project, resource, "tile source", emitterBuilder.getTileSource());
                BuilderUtil.checkFile(this.project, resource, "material", emitterBuilder.getMaterial());
                emitterBuilder.setTileSource(BuilderUtil.replaceExt(emitterBuilder.getTileSource(), "tileset", "texturesetc"));
                emitterBuilder.setTileSource(BuilderUtil.replaceExt(emitterBuilder.getTileSource(), "tilesource", "texturesetc"));
                emitterBuilder.setTileSource(BuilderUtil.replaceExt(emitterBuilder.getTileSource(), "atlas", "texturesetc"));
                emitterBuilder.setMaterial(BuilderUtil.replaceExt(emitterBuilder.getMaterial(), "material", "materialc"));
                Point3d ep = MathUtil.ddfToVecmath(emitterBuilder.getPosition());
                Quat4d er = MathUtil.ddfToVecmath(emitterBuilder.getRotation());
                for (Modifier modifier : modifiers) {
                    Modifier.Builder mb = Modifier.newBuilder(modifier);
                    Point3d p = MathUtil.ddfToVecmath(modifier.getPosition());
                    Quat4d r = MathUtil.ddfToVecmath(modifier.getRotation());
                    MathUtil.invTransform(ep, er, p);
                    mb.setPosition(MathUtil.vecmathToDDF(p));
                    MathUtil.invTransform(er, r);
                    mb.setRotation(MathUtil.vecmathToDDF(r));
                    emitterBuilder.addModifiers(mb.build());
                }
                messageBuilder.setEmitters(i, emitterBuilder.build());
            }
            messageBuilder.clearModifiers();
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = MaterialDesc.class)
    @BuilderParams(name="Material", inExts=".material", outExt=".materialc")
    public static class MaterialBuilder extends ProtoBuilder<MaterialDesc.Builder> {
        @Override
        protected MaterialDesc.Builder transform(IResource resource, MaterialDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "vertex program", messageBuilder.getVertexProgram());
            messageBuilder.setVertexProgram(BuilderUtil.replaceExt(messageBuilder.getVertexProgram(), ".vp", ".vpc"));
            BuilderUtil.checkFile(this.project, resource, "fragment program", messageBuilder.getFragmentProgram());
            messageBuilder.setFragmentProgram(BuilderUtil.replaceExt(messageBuilder.getFragmentProgram(), ".fp", ".fpc"));
            return messageBuilder;
        }
    }

    @ProtoParams(messageClass = SoundDesc.class)
    @BuilderParams(name="SoundDesc", inExts=".sound", outExt=".soundc")
    public static class SoundDescBuilder extends ProtoBuilder<SoundDesc.Builder> {
        @Override
        protected SoundDesc.Builder transform(IResource resource, SoundDesc.Builder messageBuilder)
                throws IOException, CompileExceptionError {
            BuilderUtil.checkFile(this.project, resource, "sound", messageBuilder.getSound());
            messageBuilder.setSound(BuilderUtil.replaceExt(messageBuilder.getSound(), "wav", "wavc"));
            messageBuilder.setSound(BuilderUtil.replaceExt(messageBuilder.getSound(), "ogg", "oggc"));
            return messageBuilder;
        }
    }


}
