package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.eclipse.osgi.util.NLS;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
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
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionShape;
import com.dynamo.physics.proto.Physics.CollisionShape.Shape;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.tile.proto.Tile.TileGrid;

public class ProtoBuilders {

    static String replaceTextureName(String str) {
        return BuilderUtil.replaceExt(BuilderUtil.replaceExt(str, ".png", ".texturec"), ".tga", ".texturec");
    }

    @ProtoParams(messageClass = CollectionDesc.class)
    @BuilderParams(name="Collection", inExts=".collection", outExt=".collectionc")
    public static class CollectionBuilder extends ProtoBuilder<CollectionDesc.Builder> {

        @Override
        protected CollectionDesc.Builder transform(IResource resource, CollectionDesc.Builder messageBuilder) throws CompileExceptionError {

            for (int i = 0; i < messageBuilder.getInstancesCount(); ++i) {
                InstanceDesc.Builder b = InstanceDesc.newBuilder().mergeFrom(messageBuilder.getInstances(i));
                BuilderUtil.checkFile(this.project, resource, "prototype", b.getPrototype());
                b.setPrototype(BuilderUtil.replaceExt(b.getPrototype(), ".go", ".goc"));
                messageBuilder.setInstances(i, b);
            }

            for (int i = 0; i < messageBuilder.getCollectionInstancesCount(); ++i) {
                CollectionInstanceDesc.Builder b = CollectionInstanceDesc.newBuilder().mergeFrom(messageBuilder.getCollectionInstances(i));
                BuilderUtil.checkFile(this.project, resource, "collection", b.getCollection());
                b.setCollection(BuilderUtil.replaceExt(b.getCollection(), ".collection", ".collectionc"));
                messageBuilder.setCollectionInstances(i, b);
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
                        throw new CompileExceptionError(input, 0, NLS.bind(Messages.GuiBuilder_MISSING_TEXTURE, n.getTexture()));
                    }
                }

                if (n.hasFont() && n.getFont().length() > 0) {
                    if (!fontNames.contains(n.getFont())) {
                        throw new CompileExceptionError(input, 0, NLS.bind(Messages.GuiBuilder_MISSING_FONT, n.getFont()));
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
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tileset", "tilesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tilesource", "tilesetc"));
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
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tileset", "tilesetc"));
            messageBuilder.setTileSet(BuilderUtil.replaceExt(messageBuilder.getTileSet(), "tilesource", "tilesetc"));
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

}
