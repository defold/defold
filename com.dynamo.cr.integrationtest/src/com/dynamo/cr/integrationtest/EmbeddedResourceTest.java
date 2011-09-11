package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.cr.scene.resource.CameraLoader;
import com.dynamo.cr.scene.resource.CollectionLoader;
import com.dynamo.cr.scene.resource.CollisionLoader;
import com.dynamo.cr.scene.resource.ConvexShapeLoader;
import com.dynamo.cr.scene.resource.LightLoader;
import com.dynamo.cr.scene.resource.PrototypeLoader;
import com.dynamo.cr.scene.resource.PrototypeResource;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.resource.SpriteLoader;
import com.dynamo.cr.scene.resource.TextureLoader;

public class EmbeddedResourceTest {

    private IProject project;
    private NullProgressMonitor monitor;
    private ResourceFactory resourceFactory;

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
        resourceFactory = new ResourceFactory(project);
        resourceFactory.addLoader("png", new TextureLoader());
        resourceFactory.addLoader("camera", new CameraLoader());
        resourceFactory.addLoader("light", new LightLoader());
        resourceFactory.addLoader("sprite", new SpriteLoader());
        resourceFactory.addLoader("collisionobject", new CollisionLoader());
        resourceFactory.addLoader("convexshape", new ConvexShapeLoader());
        resourceFactory.addLoader("go", new PrototypeLoader());
        resourceFactory.addLoader("collection", new CollectionLoader());
    }

    @Test
    public void testUniqueness() throws CoreException, IOException, CreateException {
        PrototypeResource twoEmbedded = (PrototypeResource)resourceFactory.load(new NullProgressMonitor(), "logic/two_embedded.go");
        assertNotNull(twoEmbedded);
        assertFalse(twoEmbedded.getComponentResources().get(0).equals(twoEmbedded.getComponentResources().get(1)));
        PrototypeResource oneEmbedded = (PrototypeResource)resourceFactory.load(new NullProgressMonitor(), "logic/one_embedded.go");
        assertNotNull(oneEmbedded);
        assertFalse(oneEmbedded.getComponentResources().get(0).equals(twoEmbedded.getComponentResources().get(1)));
    }
}
