package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.ltk.core.refactoring.Change;
import org.eclipse.ltk.core.refactoring.CheckConditionsOperation;
import org.eclipse.ltk.core.refactoring.PerformChangeOperation;
import org.eclipse.ltk.core.refactoring.PerformRefactoringOperation;
import org.eclipse.ltk.core.refactoring.Refactoring;
import org.eclipse.ltk.core.refactoring.RefactoringContribution;
import org.eclipse.ltk.core.refactoring.RefactoringCore;
import org.eclipse.ltk.core.refactoring.RefactoringDescriptor;
import org.eclipse.ltk.core.refactoring.RefactoringStatus;
import org.eclipse.ltk.core.refactoring.resource.DeleteResourcesDescriptor;
import org.eclipse.ltk.core.refactoring.resource.MoveResourcesDescriptor;
import org.eclipse.ltk.core.refactoring.resource.RenameResourceDescriptor;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.atlas.proto.AtlasProto.AtlasAnimation;
import com.dynamo.atlas.proto.AtlasProto.AtlasImage;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.Message;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class RefactorTest {

    IProject project;
    NullProgressMonitor monitor;
    protected INodeTypeRegistry nodeTypeRegistry;
    protected ILoaderContext loaderContext;

    @Before
    public void setUp() throws Exception {
        project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        monitor = new NullProgressMonitor();
        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        IProjectDescription pd = project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        project.setDescription(pd, monitor);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.integrationtest");
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            if (url.getFile().endsWith("/")) {
                // A path - skip
            } else {
                InputStream is = url.openStream();
                IFile file = project.getFile(path);
                IContainer parent = file.getParent();
                if (parent instanceof IFolder) {
                    IFolder folder = (IFolder) file.getParent();
                    while (!folder.exists()) {
                        folder.create(true, true, null);
                        parent = folder.getParent();
                        if (parent instanceof IFolder) {
                            folder = (IFolder) parent;
                        } else {
                            break;
                        }
                    }
                }
                file.create(is, true, monitor);
                is.close();
            }
        }
        this.nodeTypeRegistry = Activator.getDefault().getNodeTypeRegistry();
        this.loaderContext = new LoaderContext(project, nodeTypeRegistry);
    }

    private Message loadMessageFile(String filename, Builder builder) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(project.getFile(filename).getContents());
        TextFormat.merge(reader, builder);
        reader.close();
        return builder.build();
    }

    private Change perform(Change change) throws CoreException {
        PerformChangeOperation op= new PerformChangeOperation(change);
        op.run(null);
        assertTrue(op.changeExecuted());
        return op.getUndoChange();
    }

    private Change perform(RefactoringDescriptor descriptor) throws CoreException {
        RefactoringStatus status= new RefactoringStatus();
        Refactoring refactoring= descriptor.createRefactoring(status);
        assertTrue(status.isOK());

        PerformRefactoringOperation op = new PerformRefactoringOperation(refactoring, CheckConditionsOperation.ALL_CONDITIONS);
        op.run(null);
        RefactoringStatus validationStatus= op.getValidationStatus();
        assertTrue(!validationStatus.hasFatalError() && !validationStatus.hasError());
        return op.getUndoChange();
    }

    RenameResourceDescriptor rename(IPath path, String newName) {
        RefactoringContribution contribution = RefactoringCore.getRefactoringContribution(RenameResourceDescriptor.ID);
        RenameResourceDescriptor descriptor = (RenameResourceDescriptor) contribution.createDescriptor();
        descriptor.setResourcePath(path);
        descriptor.setNewName(newName);
        descriptor.setUpdateReferences(true);
        return descriptor;
    }

    DeleteResourcesDescriptor delete(IPath[] paths) {
        RefactoringContribution contribution = RefactoringCore.getRefactoringContribution(DeleteResourcesDescriptor.ID);
        DeleteResourcesDescriptor descriptor = (DeleteResourcesDescriptor) contribution.createDescriptor();
        descriptor.setResourcePaths(paths);
        return descriptor;
    }

    private interface ReferenceFetcher<Desc> {
        String[] getReferences(Desc desc);
    }

    @SuppressWarnings("unchecked")
    private <Desc> void testRename(Builder builder, String referer, String referee, ReferenceFetcher<Desc> referenceFetcher) throws IOException, CoreException {
        Builder postBuilder = builder.clone();
        Desc preReferer = (Desc) loadMessageFile(referer, builder);
        for (String s : referenceFetcher.getReferences(preReferer)) {
            assertEquals(referee, s);
        }

        IFile refereeFile = project.getFile(referee);
        assertTrue(refereeFile.exists());

        IPath refereePath = new Path(referee);
        String newRefereeName = refereePath.removeFileExtension().lastSegment() + "2." + refereePath.getFileExtension();
        IPath newRefereePath = refereePath.removeLastSegments(1).append(newRefereeName);
        RenameResourceDescriptor descriptor = rename(refereeFile.getFullPath(), newRefereeName);
        Change undoChange = perform(descriptor);

        Desc postReferer = (Desc) loadMessageFile(referer, postBuilder);
        for (String s : referenceFetcher.getReferences(postReferer)) {
            assertEquals(newRefereePath.toString(), s);
        }

        perform(undoChange);
    }

    @SuppressWarnings("unchecked")
    private <Desc> void testDelete(Builder builder, String referer, String referee, ReferenceFetcher<Desc> referenceFetcher) throws IOException, CoreException {
        Builder postBuilder = builder.clone();
        Desc preReferer = (Desc) loadMessageFile(referer, builder);
        for (String s : referenceFetcher.getReferences(preReferer)) {
            assertEquals(referee, s);
        }

        IFile refereeFile = project.getFile(referee);
        assertTrue(refereeFile.exists());

        DeleteResourcesDescriptor descriptor = delete(new IPath[] { refereeFile.getFullPath() });
        Change undoChange = perform(descriptor);

        Desc postReferer = (Desc) loadMessageFile(referer, postBuilder);
        for (String s : referenceFetcher.getReferences(postReferer)) {
            assertEquals("", s);
        }

        perform(undoChange);
    }

    private <Desc> void testRenameAndDelete(Builder builder, String referer, String referee, ReferenceFetcher<Desc> referenceFetcher) throws IOException, CoreException {
        testRename(builder.clone(), referer, referee, referenceFetcher);
        testDelete(builder.clone(), referer, referee, referenceFetcher);
    }

    @SuppressWarnings("unchecked")
    private <Desc> void testClearDelete(Builder builder, String referer, String referee, ReferenceFetcher<Desc> referenceFetcher) throws IOException, CoreException {
        Builder postBuilder = builder.clone();
        Desc preReferer = (Desc) loadMessageFile(referer, builder);
        for (String s : referenceFetcher.getReferences(preReferer)) {
            assertEquals(referee, s);
        }

        IFile refereeFile = project.getFile(referee);
        assertTrue(refereeFile.exists());

        DeleteResourcesDescriptor descriptor = delete(new IPath[] { refereeFile.getFullPath() });
        Change undoChange = perform(descriptor);

        Desc postReferer = (Desc) loadMessageFile(referer, postBuilder);
        assertEquals(0, referenceFetcher.getReferences(postReferer).length);

        perform(undoChange);
    }

    private <Desc> void testRenameAndClearDelete(Builder builder, String referer, String referee, ReferenceFetcher<Desc> referenceFetcher) throws IOException, CoreException {
        testRename(builder.clone(), referer, referee, referenceFetcher);
        testClearDelete(builder.clone(), referer, referee, referenceFetcher);
    }

    /*
     * Tileset
     */

    @Test
    public void testImageForTileset() throws CoreException, IOException {

        testRenameAndDelete(TileSet.newBuilder(), "graphics/sprites.tileset", "/graphics/ball.png", new ReferenceFetcher<TileSet>() {
            @Override
            public String[] getReferences(TileSet desc) {
                return new String[] {desc.getImage()};
            }
        });
    }

    @Test
    public void testCollisionForTileset() throws CoreException, IOException {

        testRenameAndDelete(TileSet.newBuilder(), "graphics/sprites.tileset", "/graphics/pow.png", new ReferenceFetcher<TileSet>() {
            @Override
            public String[] getReferences(TileSet desc) {
                return new String[] {desc.getCollision()};
            }
        });
    }

    /*
     * Atlas
     */

    @Test
    public void testImageForAtlas() throws CoreException, IOException {

        testRenameAndClearDelete(Atlas.newBuilder(), "graphics/atlas.atlas", "/graphics/ball.png", new ReferenceFetcher<Atlas>() {
            @Override
            public String[] getReferences(Atlas atlas) {
                List<String> images = new ArrayList<String>();
                for (AtlasImage image : atlas.getImagesList()) {
                    images.add(image.getImage());
                }
                for (AtlasAnimation animation : atlas.getAnimationsList()) {
                    for (AtlasImage image : animation.getImagesList()) {
                        images.add(image.getImage());
                    }
                }
                return images.toArray(new String[images.size()]);
            }
        });
    }

    @Test
    public void testAnimationImageForAtlas() throws CoreException, IOException {


        testRenameAndDelete(Atlas.newBuilder(), "graphics/atlas.atlas", "/graphics/ball.png", new ReferenceFetcher<Atlas>() {
            @Override
            public String[] getReferences(Atlas atlas) {
                List<AtlasAnimation> animationList = atlas.getAnimationsList();
                if (animationList.size() == 1 && animationList.get(0).getImagesList().size() == 1) {
                    return new String[] {animationList.get(0).getImages(0).getImage()};
                } else {
                    return new String[] {};
                }
            }
        });
    }

    /*
     * Collections
     */

    @Test
    public void testGoForCollection() throws CoreException, IOException {
        testRenameAndDelete(CollectionDesc.newBuilder(), "logic/session/base_level.collection", "/logic/session/wall.go", new ReferenceFetcher<CollectionDesc>() {
            @Override
            public String[] getReferences(CollectionDesc desc) {
                return new String[] {
                        desc.getInstances(0).getPrototype(),
                        desc.getInstances(3).getPrototype()
                };
            }
        });
    }

    @Test
    public void testCollectionForCollection() throws CoreException, IOException {
        testRenameAndDelete(CollectionDesc.newBuilder(), "logic/session/level01.collection", "/logic/session/base_level.collection", new ReferenceFetcher<CollectionDesc>() {
            @Override
            public String[] getReferences(CollectionDesc desc) {
                return new String[] { desc.getCollectionInstances(0).getCollection() };
            }
        });
    }

    @Test
    public void testDeleteRelatedFiles() throws Exception {
        IFile main_go = project.getFile("logic/main.go");
        assertTrue(main_go.exists());

        IFile main_collection = project.getFile("logic/main.collection");
        assertTrue(main_collection.exists());

        DeleteResourcesDescriptor descriptor = delete(new IPath[] { main_go.getFullPath(), main_collection.getFullPath() });
        Change undoChange = perform(descriptor);

        assertTrue(!main_go.exists());
        assertTrue(!main_collection.exists());

        perform(undoChange);

        assertTrue(main_go.exists());
        assertTrue(main_collection.exists());
    }

    /*
     * Render
     */

    @Test
    public void testRenderScriptForRender() throws CoreException, IOException {
        testRenameAndDelete(RenderPrototypeDesc.newBuilder(), "logic/default.render", "/logic/default.render_script", new ReferenceFetcher<RenderPrototypeDesc>() {
            @Override
            public String[] getReferences(RenderPrototypeDesc desc) {
                return new String[] { desc.getScript() };
            }
        });
    }

    @Test
    public void testMaterialForRender() throws CoreException, IOException {
        testRenameAndDelete(RenderPrototypeDesc.newBuilder(), "logic/default.render", "/logic/test.material", new ReferenceFetcher<RenderPrototypeDesc>() {
            @Override
            public String[] getReferences(RenderPrototypeDesc desc) {
                return new String[] { desc.getMaterials(0).getMaterial() };
            }
        });
    }

    /*
     * Components and prototypes
     */

    @Test
    public void testCameraForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/main.go", "/logic/test.camera", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(2).getComponent() };
            }
        });
    }

    @Test
    public void testCollectionForCollectionProxy() throws CoreException, IOException {
        testRenameAndDelete(CollectionProxyDesc.newBuilder(), "logic/session/level01.collectionproxy", "/logic/session/level01.collection", new ReferenceFetcher<CollectionProxyDesc>() {
            @Override
            public String[] getReferences(CollectionProxyDesc desc) {
                return new String[] { desc.getCollection() };
            }
        });
    }

    @Test
    public void testCollectionProxyForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/session.go", "/logic/session/level01.collectionproxy", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(1).getComponent() };
            }
        });
    }

    @Test
    public void testConvexShapeForCollisionObject() throws CoreException, IOException {
        testRenameAndDelete(CollisionObjectDesc.newBuilder(), "logic/session/paddle.collisionobject", "/logic/session/paddle.convexshape", new ReferenceFetcher<CollisionObjectDesc>() {
            @Override
            public String[] getReferences(CollisionObjectDesc desc) {
                return new String[] { desc.getCollisionShape() };
            }
        });
    }

    @Test
    public void testTileGridForCollisionObject() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/tilegrid_collision.go", "/tilegrid/test.collisionobject", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(0).getComponent() };
            }
        });
    }

    @Test
    public void testCollisionObjectForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/paddle.go", "/logic/session/paddle.collisionobject", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(3).getComponent() };
            }
        });
    }

    @Test
    public void testGuiForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/hud.go", "/logic/session/hud.gui", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(0).getComponent() };
            }
        });
    }

    @Test
    public void testLightForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/main.go", "/logic/test.light", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(3).getComponent() };
            }
        });
    }

    @Test
    public void testModelForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "graphics/box.go", "/graphics/box.model", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(0).getComponent() };
            }
        });
    }

    @Test
    public void testScriptForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/ball.go", "/logic/session/ball.script", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(1).getComponent() };
            }
        });
    }

    @Test
    public void testRenameEmbeddedWav() throws CoreException, IOException {
        testRename(CollectionDesc.newBuilder(), "collection/embedded_embedded_sounds.collection", "/sounds/powerup.wav", new ReferenceFetcher<CollectionDesc>() {
            @Override
            public String[] getReferences(CollectionDesc desc) {
                try {
                    PrototypeDesc.Builder b = PrototypeDesc.newBuilder();
                    TextFormat.merge(desc.getEmbeddedInstances(0).getData(), b);
                    SoundDesc.Builder c = SoundDesc.newBuilder();
                    TextFormat.merge(b.getEmbeddedComponents(0).getData(), c);
                    return new String[] { c.getSound() };
                } catch (TextFormat.ParseException e) {
                    assertTrue(false);
                    return null;
                }
            }
        });
    }

    @Test
    public void testRenameEmbeddedOgg() throws CoreException, IOException {
        testRename(CollectionDesc.newBuilder(), "collection/embedded_embedded_sounds.collection", "/sounds/drums.ogg", new ReferenceFetcher<CollectionDesc>() {
            @Override
            public String[] getReferences(CollectionDesc desc) {
                try {
                    PrototypeDesc.Builder b = PrototypeDesc.newBuilder();
                    TextFormat.merge(desc.getEmbeddedInstances(1).getData(), b);
                    SoundDesc.Builder c = SoundDesc.newBuilder();
                    TextFormat.merge(b.getEmbeddedComponents(0).getData(), c);
                    return new String[] { c.getSound() };
                } catch (TextFormat.ParseException e) {
                    assertTrue(false);
                    return null;
                }
            }
        });
    }

    @Test
    public void testSpriteForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/ball.go", "/logic/session/ball.sprite", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(0).getComponent() };
            }
        });
    }

    @Test
    public void testSpineModelForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/ball.go", "/spine/reload.spinemodel", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(3).getComponent() };
            }
        });
    }

    @Test
    public void testWAVForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/ball.go", "/sounds/tink.wav", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(2).getComponent() };
            }
        });
    }

    @Test
    public void testTileGridForGO() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/tilegrid.go", "/tilegrid/test.tilegrid", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                return new String[] { desc.getComponents(0).getComponent() };
            }
        });
    }

    @Test
    public void testCollectionForEmbeddedCollectionProxy() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/main.go", "/logic/session/session.collection", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
                try {
                    TextFormat.merge(desc.getEmbeddedComponents(0).getData(), builder);
                    CollisionObjectDesc collisionObjectDesc = builder.build();
                    return new String[] { collisionObjectDesc.getCollisionShape() };
                } catch (ParseException e) {
                    return new String[] {};
                }
            }
        });
    }

    @Test
    public void testConvexShapeForEmbeddedCollisionObject() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/block.go", "/logic/session/block.convexshape", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
                try {
                    TextFormat.merge(desc.getEmbeddedComponents(0).getData(), builder);
                    CollisionObjectDesc collisionObjectDesc = builder.build();
                    return new String[] { collisionObjectDesc.getCollisionShape() };
                } catch (ParseException e) {
                    return new String[] {};
                }
            }
        });
    }

    @Test
    public void testTileGridForEmbeddedCollisionObject() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/tilegrid_embedded_collision.go", "/tilegrid/test.tilegrid", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
                try {
                    TextFormat.merge(desc.getEmbeddedComponents(0).getData(), builder);
                    CollisionObjectDesc collisionObjectDesc = builder.build();
                    return new String[] { collisionObjectDesc.getCollisionShape() };
                } catch (ParseException e) {
                    return new String[] {};
                }
            }
        });
    }

    @Test
    public void testGOForEmbeddedFactory() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/session/ball.go", "/logic/session/pow.go", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                FactoryDesc.Builder builder = FactoryDesc.newBuilder();
                try {
                    TextFormat.merge(desc.getEmbeddedComponents(1).getData(), builder);
                    FactoryDesc factoryDesc = builder.build();
                    return new String[] { factoryDesc.getPrototype() };
                } catch (ParseException e) {
                    return new String[] {};
                }
            }
        });
    }

    @Test
    public void testTileSetForEmbeddedSprite() throws CoreException, IOException {
        testRenameAndDelete(PrototypeDesc.newBuilder(), "logic/embedded_sprite.go", "/tileset/test.tileset", new ReferenceFetcher<PrototypeDesc>() {
            @Override
            public String[] getReferences(PrototypeDesc desc) {
                SpriteDesc.Builder builder = SpriteDesc.newBuilder();
                try {
                    TextFormat.merge(desc.getEmbeddedComponents(0).getData(), builder);
                    SpriteDesc spriteDesc = builder.build();
                    return new String[] { spriteDesc.getTileSet() };
                } catch (ParseException e) {
                    return new String[] {};
                }
            }
        });
    }

    /*
     * Spine Scene
     */

    @Test
    public void testSpineJsonForSpineScene() throws CoreException, IOException {
        testRenameAndDelete(SpineSceneDesc.newBuilder(), "spine/reload.spinescene", "/spine/test_spine.json", new ReferenceFetcher<SpineSceneDesc>() {
            @Override
            public String[] getReferences(SpineSceneDesc desc) {
                return new String[] { desc.getSpineJson() };
            }
        });
    }

    @Test
    public void testAtlasForSpineScene() throws CoreException, IOException {
        testRenameAndDelete(SpineSceneDesc.newBuilder(), "spine/reload.spinescene", "/graphics/atlas.atlas", new ReferenceFetcher<SpineSceneDesc>() {
            @Override
            public String[] getReferences(SpineSceneDesc desc) {
                return new String[] { desc.getAtlas() };
            }
        });
    }

    /*
     * Spine Model
     */

    @Test
    public void testSpineSceneForSpineModel() throws CoreException, IOException {
        testRenameAndDelete(SpineModelDesc.newBuilder(), "spine/reload.spinemodel", "/spine/reload.spinescene", new ReferenceFetcher<SpineModelDesc>() {
            @Override
            public String[] getReferences(SpineModelDesc desc) {
                return new String[] { desc.getSpineScene() };
            }
        });
    }

    @Test
    public void testMaterialForSpineModel() throws CoreException, IOException {
        testRenameAndDelete(SpineModelDesc.newBuilder(), "spine/reload.spinemodel", "/sprite/sprite.material", new ReferenceFetcher<SpineModelDesc>() {
            @Override
            public String[] getReferences(SpineModelDesc desc) {
                return new String[] { desc.getMaterial() };
            }
        });
    }

    /*
     * Fonts
     */

    @Test
    public void testTTFForFont() throws CoreException, IOException {
        testRenameAndDelete(FontDesc.newBuilder(), "fonts/active_menu_item.font", "/fonts/justov.ttf", new ReferenceFetcher<FontDesc>() {
            @Override
            public String[] getReferences(FontDesc desc) {
                return new String[] { desc.getFont() };
            }
        });
    }

    @Test
    public void testMaterialForFont() throws CoreException, IOException {
        testRenameAndDelete(FontDesc.newBuilder(), "fonts/active_menu_item.font", "/fonts/active_menu_item.material", new ReferenceFetcher<FontDesc>() {
            @Override
            public String[] getReferences(FontDesc desc) {
                return new String[] { desc.getMaterial() };
            }
        });
    }

    /*
     * Materials
     */

    @Test
    public void testVPForMaterial() throws CoreException, IOException {
        testRenameAndDelete(MaterialDesc.newBuilder(), "fonts/active_menu_item.material", "/fonts/font.vp", new ReferenceFetcher<MaterialDesc>() {
            @Override
            public String[] getReferences(MaterialDesc desc) {
                return new String[] { desc.getVertexProgram() };
            }
        });
    }

    @Test
    public void testFPForMaterial() throws CoreException, IOException {
        testRenameAndDelete(MaterialDesc.newBuilder(), "fonts/active_menu_item.material", "/fonts/font.fp", new ReferenceFetcher<MaterialDesc>() {
            @Override
            public String[] getReferences(MaterialDesc desc) {
                return new String[] { desc.getFragmentProgram() };
            }
        });
    }

    /*
     * Gui
     */

    @Test
    public void testGuiScriptForGui() throws CoreException, IOException {
        testRenameAndDelete(SceneDesc.newBuilder(), "logic/main.gui", "/logic/main.gui_script", new ReferenceFetcher<SceneDesc>() {
            @Override
            public String[] getReferences(SceneDesc desc) {
                return new String[] { desc.getScript() };
            }
        });
    }

    @Test
    public void testFontForGui() throws CoreException, IOException {
        testRenameAndDelete(SceneDesc.newBuilder(), "logic/main.gui", "/fonts/menu_item.font", new ReferenceFetcher<SceneDesc>() {
            @Override
            public String[] getReferences(SceneDesc desc) {
                return new String[] { desc.getFonts(0).getFont() };
            }
        });
    }

    @Test
    public void testTextureForGui() throws CoreException, IOException {
        testRenameAndDelete(SceneDesc.newBuilder(), "logic/main.gui", "/graphics/ball.png", new ReferenceFetcher<SceneDesc>() {
            @Override
            public String[] getReferences(SceneDesc desc) {
                return new String[] { desc.getTextures(0).getTexture() };
            }
        });
    }

    /*
     * Model
     */

    @Test
    public void testMeshForModel() throws CoreException, IOException {
        testRenameAndDelete(ModelDesc.newBuilder(), "graphics/box.model", "/graphics/box.dae", new ReferenceFetcher<ModelDesc>() {
            @Override
            public String[] getReferences(ModelDesc desc) {
                return new String[] { desc.getMesh() };
            }
        });
    }

    @Test
    public void testMaterialForModel() throws CoreException, IOException {
        testRenameAndDelete(ModelDesc.newBuilder(), "graphics/box.model", "/graphics/simple.material", new ReferenceFetcher<ModelDesc>() {
            @Override
            public String[] getReferences(ModelDesc desc) {
                return new String[] { desc.getMaterial() };
            }
        });
    }

    @Test
    public void testTextureForModel() throws CoreException, IOException {
        testRenameAndDelete(ModelDesc.newBuilder(), "graphics/box.model", "/graphics/block.png", new ReferenceFetcher<ModelDesc>() {
            @Override
            public String[] getReferences(ModelDesc desc) {
                return new String[] { desc.getTextures(0) };
            }
        });
    }

    /*
     * Folder and complex renames
     */

    @Test
    public void testRenameGraphicsFolder() throws CoreException, IOException {
        // Rename the graphics folder. ie rename and update N files
        SceneDesc preGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("/graphics/left_hud.png", preGui.getTextures(0).getTexture());

        IFolder graphics = project.getFolder("graphics");

        RenameResourceDescriptor descriptor = rename(graphics.getFullPath(), "graphics2");
        Change undoChange = perform(descriptor);

        SceneDesc postGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("/graphics2/left_hud.png", postGui.getTextures(0).getTexture());

        perform(undoChange);
    }

    @Test
    public void testRenameFontFolder() throws CoreException, IOException {
        // Rename the fonts folder. ie rename and update N files
        SceneDesc preGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("/fonts/vera_mo_bd.font", preGui.getFonts(0).getFont());

        IFolder fonts = project.getFolder("fonts");

        RenameResourceDescriptor descriptor = rename(fonts.getFullPath(), "fonts2");
        Change undoChange = perform(descriptor);

        SceneDesc postGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("/fonts2/vera_mo_bd.font", postGui.getFonts(0).getFont());

        perform(undoChange);
    }

    @Test
    public void testOverlappingRenameUpdate() throws CoreException, IOException {
        // Rename logic/session to logic/session2
        // Ensure that overlapping changes works (rename and file-changes on the same sub-tree)
        PrototypeDesc preBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        assertEquals("/logic/session/ball.sprite", preBallGo.getComponents(0).getComponent());

        IFolder session = project.getFolder("logic/session");

        RenameResourceDescriptor descriptor = rename(session.getFullPath(), "session2");
        Change undoChange = perform(descriptor);

        PrototypeDesc postBallGo = (PrototypeDesc) loadMessageFile("logic/session2/ball.go", PrototypeDesc.newBuilder());
        assertEquals("/logic/session2/ball.sprite", postBallGo.getComponents(0).getComponent());

        perform(undoChange);
    }

    @Test
    public void testDependentMove() throws CoreException, IOException {
        IFolder newDest = project.getFolder("logic/session/new_dest");
        newDest.create(true, true, new NullProgressMonitor());

        IFile ballGo = project.getFile("logic/session/ball.go");
        IFile ballScript = project.getFile("logic/session/ball.script");

        RefactoringContribution contribution = RefactoringCore.getRefactoringContribution(MoveResourcesDescriptor.ID);
        MoveResourcesDescriptor descriptor = (MoveResourcesDescriptor) contribution.createDescriptor();
        descriptor.setResourcesToMove(new IResource[] {ballScript, ballGo});
        descriptor.setDestination(newDest);
        descriptor.setUpdateReferences(true);

        perform(descriptor);

        PrototypeDesc ballGoPrototypeDesc = (PrototypeDesc) loadMessageFile("logic/session/new_dest/ball.go", PrototypeDesc.newBuilder());
        assertEquals("/logic/session/new_dest/ball.script", ballGoPrototypeDesc.getComponents(1).getComponent());
    }
}

