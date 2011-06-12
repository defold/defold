package com.dynamo.cr.scene.test.graph;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.NodeFactory;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.test.util.SceneContext;

public class ReloadTest {
    private NodeFactory nodeFactory;
    private ResourceFactory resourceFactory;
    private Scene scene;

    static {
        // Fix for strange hang on osx in System#loadLibrary
        System.setProperty("java.awt.headless", "true");
    }

    @Before
    public void setup() throws CoreException, IOException {
        SceneContext context = new SceneContext();
        this.nodeFactory = context.nodeFactory;
        this.resourceFactory = context.resourceFactory;
        this.scene = context.scene;
    }

    @Test
    public void testReloadCollection() throws Exception {
        // TODO: This test only tests a bug related to reloading at the moment, should be turned into a proper reload test
        String name = "test.collection";

        Resource resource = resourceFactory.load(new NullProgressMonitor(), name);
        Node node = nodeFactory.create(name, resource, null, scene);
        assertThat(node, instanceOf(CollectionNode.class));
        assertThat(node.getChildren().length, is(3));
        assertTrue(node.isOk());

        // Fake resource changed
        resource.fireChanged();

        assertTrue(node.isOk());
    }
}
