package com.dynamo.cr.scene.test.util;

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
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.osgi.framework.Bundle;

import com.dynamo.cr.scene.graph.CameraNode;
import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.CollisionNode;
import com.dynamo.cr.scene.graph.LightNode;
import com.dynamo.cr.scene.graph.ModelNode;
import com.dynamo.cr.scene.graph.NodeFactory;
import com.dynamo.cr.scene.graph.PrototypeNode;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.graph.SpriteNode;
import com.dynamo.cr.scene.resource.CameraLoader;
import com.dynamo.cr.scene.resource.CollectionLoader;
import com.dynamo.cr.scene.resource.CollisionLoader;
import com.dynamo.cr.scene.resource.ConvexShapeLoader;
import com.dynamo.cr.scene.resource.LightLoader;
import com.dynamo.cr.scene.resource.MeshLoader;
import com.dynamo.cr.scene.resource.ModelLoader;
import com.dynamo.cr.scene.resource.PrototypeLoader;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.resource.SpriteLoader;
import com.dynamo.cr.scene.resource.TextureLoader;

public class SceneContext {
    public NodeFactory nodeFactory;
    public ResourceFactory resourceFactory;
    public Scene scene;
    public IProject project;

    public SceneContext() throws CoreException, IOException {
        String root = "test";
        this.project = ResourcesPlugin.getWorkspace().getRoot().getProject(root);
        IProgressMonitor monitor = new NullProgressMonitor();

        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        IProjectDescription pd = project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        project.setDescription(pd, monitor);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.scene.test");
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

        resourceFactory = new ResourceFactory(this.project);
        resourceFactory.addLoader("collection", new CollectionLoader());
        resourceFactory.addLoader("png", new TextureLoader());
        resourceFactory.addLoader("camera", new CameraLoader());
        resourceFactory.addLoader("light", new LightLoader());
        resourceFactory.addLoader("sprite", new SpriteLoader());
        resourceFactory.addLoader("collisionobject", new CollisionLoader());
        resourceFactory.addLoader("convexshape", new ConvexShapeLoader());
        resourceFactory.addLoader("go", new PrototypeLoader());
        resourceFactory.addLoader("model", new ModelLoader());
        resourceFactory.addLoader("dae", new MeshLoader());
        nodeFactory = new NodeFactory();
        nodeFactory.addCreator("collection", CollectionNode.getCreator());
        nodeFactory.addCreator("go", PrototypeNode.getCreator());
        nodeFactory.addCreator("model", ModelNode.getCreator());
        nodeFactory.addCreator("camera", CameraNode.getCreator());
        nodeFactory.addCreator("light", LightNode.getCreator());
        nodeFactory.addCreator("sprite", SpriteNode.getCreator());
        nodeFactory.addCreator("collisionobject", CollisionNode.getCreator());
        scene = new Scene();
    }
}
