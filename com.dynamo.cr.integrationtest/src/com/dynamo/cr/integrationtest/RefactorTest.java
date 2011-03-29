package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Enumeration;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
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
import org.eclipse.ltk.core.refactoring.resource.RenameResourceDescriptor;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.Message;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;

public class RefactorTest {

    private IProject project;
    private NullProgressMonitor monitor;

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
        @SuppressWarnings("unchecked")
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                project.getFolder(path).create(true, true, monitor);
            } else {
                InputStream is = url.openStream();
                IFile file = project.getFile(path);
                file.create(is, true, monitor);
                is.close();
            }
        }
    }

    Message loadMessageFile(String filename, Builder builder) throws IOException, CoreException {
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

    DeleteResourcesDescriptor delete(IPath path) {
        RefactoringContribution contribution = RefactoringCore.getRefactoringContribution(DeleteResourcesDescriptor.ID);
        DeleteResourcesDescriptor descriptor = (DeleteResourcesDescriptor) contribution.createDescriptor();
        descriptor.setResourcePaths(new IPath[] { path });
        return descriptor;
    }

    /*
     * Rename tests single files
     */
    @Test
    public void testRenameBallPng() throws CoreException, IOException {
        // Simple test. Rename ball.png -> ball2.png and check reference update
        SpriteDesc preBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());

        assertEquals("graphics/ball.png", preBallSprite.getTexture());

        IFile ball = project.getFile("graphics/ball.png");
        assertTrue(ball.exists());

        RenameResourceDescriptor descriptor = rename(ball.getFullPath(), "ball2.png");
        Change undoChange = perform(descriptor);

        SpriteDesc postBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());
        assertEquals("graphics/ball2.png", postBallSprite.getTexture());

        perform(undoChange);
    }

    @Test
    public void testRenameWall() throws CoreException, IOException {
        // Simple test. Rename wall.go -> wall2.go and check reference update
        CollectionDesc preCollection = (CollectionDesc) loadMessageFile("logic/session/base_level.collection", CollectionDesc.newBuilder());
        assertEquals("logic/session/wall.go", preCollection.getInstances(0).getPrototype());
        assertEquals("logic/session/wall.go", preCollection.getInstances(3).getPrototype());

        IFile wall = project.getFile("logic/session/wall.go");
        assertTrue(wall.exists());

        RenameResourceDescriptor descriptor = rename(wall.getFullPath(), "wall2.go");
        Change undoChange = perform(descriptor);

        CollectionDesc postCollection = (CollectionDesc) loadMessageFile("logic/session/base_level.collection", CollectionDesc.newBuilder());
        assertEquals("logic/session/wall2.go", postCollection.getInstances(0).getPrototype());
        assertEquals("logic/session/wall2.go", postCollection.getInstances(3).getPrototype());

        perform(undoChange);
    }

    @Test
    public void testRenameBaseLevelCollection() throws CoreException, IOException {
        // Simple test. Rename base_level.collection -> base_level2.collection and check reference update
        CollectionDesc preCollection = (CollectionDesc) loadMessageFile("logic/session/level01.collection", CollectionDesc.newBuilder());
        assertEquals("logic/session/base_level.collection", preCollection.getCollectionInstances(0).getCollection());

        IFile baseLevel = project.getFile("logic/session/base_level.collection");
        assertTrue(baseLevel.exists());

        RenameResourceDescriptor descriptor = rename(baseLevel.getFullPath(), "base_level2.collection");
        Change undoChange = perform(descriptor);

        CollectionDesc postCollection = (CollectionDesc) loadMessageFile("logic/session/level01.collection", CollectionDesc.newBuilder());
        assertEquals("logic/session/base_level2.collection", postCollection.getCollectionInstances(0).getCollection());

        perform(undoChange);
    }

    @Test
    public void testRenameBallSprite() throws CoreException, IOException {
        // Simple test. Rename ball.sprite to ball2.sprite. Check that ball.go is updated.
        PrototypeDesc preBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        assertEquals("logic/session/ball.sprite", preBallGo.getComponents(0).getComponent());

        IFile spriteBall = project.getFile("logic/session/ball.sprite");
        assertTrue(spriteBall.exists());

        RenameResourceDescriptor descriptor = rename(spriteBall.getFullPath(), "ball2.sprite");
        Change undoChange = perform(descriptor);

        PrototypeDesc postBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        assertEquals("logic/session/ball2.sprite", postBallGo.getComponents(0).getComponent());

        perform(undoChange);
    }

    @Test
    public void testRenameRenderScriptForRender() throws CoreException, IOException {
        // Simple test. Rename render script and check that render is updated
        RenderPrototypeDesc preRender = (RenderPrototypeDesc) loadMessageFile("logic/default.render", RenderPrototypeDesc.newBuilder());
        assertEquals("logic/default.render_script", preRender.getScript());

        IFile renderScript = project.getFile("logic/default.render_script");
        assertTrue(renderScript.exists());

        RenameResourceDescriptor descriptor = rename(renderScript.getFullPath(), "default2.render_script");
        Change undoChange = perform(descriptor);

        RenderPrototypeDesc postRender = (RenderPrototypeDesc) loadMessageFile("logic/default.render", RenderPrototypeDesc.newBuilder());
        assertEquals("logic/default2.render_script", postRender.getScript());

        perform(undoChange);
    }

    @Test
    public void testRenameMaterialForRender() throws CoreException, IOException {
        // Simple test. Rename render script and check that render is updated
        RenderPrototypeDesc preRender = (RenderPrototypeDesc) loadMessageFile("logic/default.render", RenderPrototypeDesc.newBuilder());
        assertEquals("logic/test.material", preRender.getMaterials(0).getMaterial());

        IFile material = project.getFile("logic/test.material");
        assertTrue(material.exists());

        RenameResourceDescriptor descriptor = rename(material.getFullPath(), "test2.material");
        Change undoChange = perform(descriptor);

        RenderPrototypeDesc postRender = (RenderPrototypeDesc) loadMessageFile("logic/default.render", RenderPrototypeDesc.newBuilder());
        assertEquals("logic/test2.material", postRender.getMaterials(0).getMaterial());

        perform(undoChange);
    }

    /*
     * Render files referrred by embedded components
     */
    @Test
    public void testRenameBlockConvexShape() throws CoreException, IOException {
        // Rename block.convexshape to block.convexshape. Check that block.go is updated.
        PrototypeDesc preBlockGo = (PrototypeDesc) loadMessageFile("logic/session/block.go", PrototypeDesc.newBuilder());
        CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
        TextFormat.merge(preBlockGo.getEmbeddedComponents(0).getData(), builder);
        CollisionObjectDesc preCollisionObject = builder.build();
        assertEquals("logic/session/block.convexshape", preCollisionObject.getCollisionShape());

        IFile blockConvexShape = project.getFile("logic/session/block.convexshape");
        assertTrue(blockConvexShape.exists());

        RenameResourceDescriptor descriptor = rename(blockConvexShape.getFullPath(), "block2.convexshape");
        Change undoChange = perform(descriptor);

        PrototypeDesc postBlockGo = (PrototypeDesc) loadMessageFile("logic/session/block.go", PrototypeDesc.newBuilder());
        builder = CollisionObjectDesc.newBuilder();
        TextFormat.merge(postBlockGo.getEmbeddedComponents(0).getData(), builder);
        CollisionObjectDesc postCollisionObject = builder.build();
        assertEquals("logic/session/block2.convexshape", postCollisionObject.getCollisionShape());

        perform(undoChange);
    }

    /*
     * Folder and complex renames
     */
    @Test
    public void testRenameGraphicsFolder() throws CoreException, IOException {
        // Rename the graphics folder. ie rename and update N files
        SpriteDesc preBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());
        assertEquals("graphics/ball.png", preBallSprite.getTexture());
        SceneDesc preGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("graphics/left_hud.png", preGui.getTextures(0).getTexture());

        IFolder graphics = project.getFolder("graphics");

        RenameResourceDescriptor descriptor = rename(graphics.getFullPath(), "graphics2");
        Change undoChange = perform(descriptor);

        SpriteDesc postBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());
        assertEquals("graphics2/ball.png", postBallSprite.getTexture());
        SceneDesc postGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("graphics2/left_hud.png", postGui.getTextures(0).getTexture());

        perform(undoChange);
    }

    @Test
    public void testRenameFontFolder() throws CoreException, IOException {
        // Rename the fonts folder. ie rename and update N files
        SceneDesc preGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("fonts/vera_mo_bd.font", preGui.getFonts(0).getFont());

        IFolder fonts = project.getFolder("fonts");

        RenameResourceDescriptor descriptor = rename(fonts.getFullPath(), "fonts2");
        Change undoChange = perform(descriptor);

        SceneDesc postGui = (SceneDesc) loadMessageFile("logic/session/hud.gui", SceneDesc.newBuilder());
        assertEquals("fonts2/vera_mo_bd.font", postGui.getFonts(0).getFont());

        perform(undoChange);
    }

    @Test
    public void testOverlappingRenameUpdate() throws CoreException, IOException {
        // Rename logic/session to logic/session2
        // Ensure that overlapping changes works (rename and file-changes on the same sub-tree)
        PrototypeDesc preBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        assertEquals("logic/session/ball.sprite", preBallGo.getComponents(0).getComponent());

        IFolder session = project.getFolder("logic/session");

        RenameResourceDescriptor descriptor = rename(session.getFullPath(), "session2");
        Change undoChange = perform(descriptor);

        PrototypeDesc postBallGo = (PrototypeDesc) loadMessageFile("logic/session2/ball.go", PrototypeDesc.newBuilder());
        assertEquals("logic/session2/ball.sprite", postBallGo.getComponents(0).getComponent());

        perform(undoChange);
    }

    /*
     * Test delete components
     */
    public void genericComponentDelete(String goFileName, String componentFileName) throws CoreException, IOException {
        // Delete component. Check that #components is reduced by one, ie the reference to component is removed.
        PrototypeDesc preGo = (PrototypeDesc) loadMessageFile(goFileName, PrototypeDesc.newBuilder());
        int preCount = preGo.getComponentsCount();

        IFile componentFile = project.getFile(componentFileName);
        assertTrue(componentFile.exists());

        DeleteResourcesDescriptor descriptor = delete(componentFile.getFullPath());
        Change undoChange = perform(descriptor);

        PrototypeDesc postGo = (PrototypeDesc) loadMessageFile(goFileName, PrototypeDesc.newBuilder());
        int postCount = postGo.getComponentsCount();
        assertEquals(preCount-1, postCount);

        perform(undoChange);
    }

    @Test
    public void testDeleteSprite() throws CoreException, IOException {
        genericComponentDelete("logic/session/ball.go", "logic/session/ball.sprite");
    }

    @Test
    public void testDeleteScript() throws CoreException, IOException {
        genericComponentDelete("logic/session/ball.go", "logic/session/ball.script");
    }

    @Test
    public void testDeleteGui() throws CoreException, IOException {
        genericComponentDelete("logic/session/hud.go", "logic/session/hud.gui");
    }

    /*
     * Test embedded components delete, ie remove files referred from embedded component
     */
    public void genericEmbeddedComponentReferenceDelete(String goFileName, String componentFileName) throws CoreException, IOException {
        // Delete resource referred by embedded component. Check that #embedded_components is reduced by one, ie the embedded component is removed
        PrototypeDesc preGo = (PrototypeDesc) loadMessageFile(goFileName, PrototypeDesc.newBuilder());
        int preCount = preGo.getEmbeddedComponentsCount();

        IFile componentFile = project.getFile(componentFileName);
        assertTrue(componentFile.exists());

        DeleteResourcesDescriptor descriptor = delete(componentFile.getFullPath());
        Change undoChange = perform(descriptor);

        PrototypeDesc postGo = (PrototypeDesc) loadMessageFile(goFileName, PrototypeDesc.newBuilder());
        int postCount = postGo.getEmbeddedComponentsCount();
        assertEquals(preCount-1, postCount);

        perform(undoChange);
    }

    @Test
    public void testEmbeddedDeleteConvexShape() throws CoreException, IOException {
        genericEmbeddedComponentReferenceDelete("logic/session/ball.go", "logic/session/ball.convexshape");
    }

    @Test
    public void testEmbeddedDeleteSpawnPointReference() throws CoreException, IOException {
        genericEmbeddedComponentReferenceDelete("logic/session/ball.go", "logic/session/pow.go");
    }

}

