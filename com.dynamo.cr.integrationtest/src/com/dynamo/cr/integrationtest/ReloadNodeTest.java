package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.CharArrayWriter;
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

import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.cr.scene.graph.NodeFactory;
import com.dynamo.cr.scene.graph.PrototypeNode;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.resource.CameraLoader;
import com.dynamo.cr.scene.resource.CollectionLoader;
import com.dynamo.cr.scene.resource.CollisionLoader;
import com.dynamo.cr.scene.resource.ConvexShapeLoader;
import com.dynamo.cr.scene.resource.LightLoader;
import com.dynamo.cr.scene.resource.PrototypeLoader;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.resource.SpriteLoader;
import com.dynamo.cr.scene.resource.TextureLoader;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;

public class ReloadNodeTest {

    private IProject project;
    private NullProgressMonitor monitor;
    private ResourceFactory resourceFactory;
    private NodeFactory nodeFactory;
    private Scene scene;

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
        resourceFactory = new ResourceFactory(project);
        resourceFactory.addLoader("png", new TextureLoader());
        resourceFactory.addLoader("camera", new CameraLoader());
        resourceFactory.addLoader("light", new LightLoader());
        resourceFactory.addLoader("sprite", new SpriteLoader());
        resourceFactory.addLoader("collisionobject", new CollisionLoader());
        resourceFactory.addLoader("convexshape", new ConvexShapeLoader());
        resourceFactory.addLoader("go", new PrototypeLoader());
        resourceFactory.addLoader("collection", new CollectionLoader());
        ResourcesPlugin.getWorkspace().addResourceChangeListener(resourceFactory);
        nodeFactory = new NodeFactory();
        nodeFactory.addCreator("go", PrototypeNode.getCreator());
        nodeFactory.addCreator("collection", CollectionNode.getCreator());
        scene = new Scene();
    }

    @Test
    public void testSetup() {
        assertNotNull(project);
        assertNotNull(monitor);
        assertNotNull(resourceFactory);
        assertNotNull(scene);
    }

    @Test
    public void testPrototypeNode() throws CoreException, IOException, CreateException {
        String path = "logic/main.go";
        Resource resource = resourceFactory.load(new NullProgressMonitor(), path);
        assertNotNull(resource);
        PrototypeNode main = (PrototypeNode)nodeFactory.create("main", resource, null, this.scene);
        assertTrue(main.getChildren().length > 1);
        assertNotNull(main);
        ComponentDesc.Builder compBuilder = ComponentDesc.newBuilder();
        PrototypeDesc.Builder protoBuilder = PrototypeDesc.newBuilder();
        PrototypeDesc desc = protoBuilder.addComponents(compBuilder.setId("test").setComponent("test_path").build()).build();
        IFile file = this.project.getFile(path);
        assertTrue(file.exists());
        CharArrayWriter output = new CharArrayWriter();
        TextFormat.print(desc, output);
        output.close();
        file.setContents(new ByteArrayInputStream(output.toString().getBytes()), 0, new NullProgressMonitor());
        assertTrue(main.getChildren().length == 1);
    }

    @Test
    public void testCollectionNode() throws CoreException, IOException, CreateException {
        String path = "logic/main.collection";
        Resource resource = resourceFactory.load(new NullProgressMonitor(), path);
        assertNotNull(resource);
        CollectionNode main = (CollectionNode)nodeFactory.create("main", resource, null, this.scene);
        assertTrue(main.getChildren().length == 1);
        assertNotNull(main);
        CollectionDesc.Builder builder = CollectionDesc.newBuilder();
        CollectionDesc desc = builder.setName("name").build();
        IFile file = this.project.getFile(path);
        assertTrue(file.exists());
        CharArrayWriter output = new CharArrayWriter();
        TextFormat.print(desc, output);
        output.close();
        file.setContents(new ByteArrayInputStream(output.toString().getBytes()), 0, new NullProgressMonitor());
        assertTrue(main.getChildren().length == 0);
    }
}
