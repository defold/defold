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
import org.eclipse.ltk.core.refactoring.resource.RenameResourceDescriptor;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
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
        return descriptor;
    }

    @Test
    public void testSpriteTextureUpdateFile() throws CoreException, IOException {
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
    public void testSpriteTextureUpdateFolder() throws CoreException, IOException {
        SpriteDesc preBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());

        assertEquals("graphics/ball.png", preBallSprite.getTexture());

        IFolder graphics = project.getFolder("graphics");

        RenameResourceDescriptor descriptor = rename(graphics.getFullPath(), "graphics2");
        Change undoChange = perform(descriptor);

        SpriteDesc postBallSprite = (SpriteDesc) loadMessageFile("logic/session/ball.sprite", SpriteDesc.newBuilder());
        assertEquals("graphics2/ball.png", postBallSprite.getTexture());

        perform(undoChange);
    }

    @Test
    public void testGameObjectUpdateFile() throws CoreException, IOException {
        PrototypeDesc preBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        assertEquals("logic/session/ball.sprite", preBallGo.getComponents(0).getComponent());

        IFile spriteBall = project.getFile("logic/session/ball.sprite");
        assertTrue(spriteBall.exists());

        RenameResourceDescriptor descriptor = rename(spriteBall.getFullPath(), "ball2.sprite");
        Change undoChange = perform(descriptor);

        PrototypeDesc postBallGo = (PrototypeDesc) loadMessageFile("logic/session/ball.go", PrototypeDesc.newBuilder());
        /*System.out.println(preBallGo);
        System.out.println("--------------");
        System.out.println(postBallGo);*/
        assertEquals("logic/session/ball2.sprite", postBallGo.getComponents(0).getComponent());

        perform(undoChange);
    }


}

