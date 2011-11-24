package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;

import org.eclipse.core.resources.IFile;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.LoaderContext;

public class NodeTest extends IntegrationTest {

    private Activator nodeTypeRegistry;
    private LoaderContext loaderContext;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        this.nodeTypeRegistry = Activator.getDefault();
        this.loaderContext = new LoaderContext(project, nodeTypeRegistry, mock(ILogger.class));
    }

    @Test
    public void testCollisionObject() throws Exception {
        INodeLoader loader = nodeTypeRegistry.getLoader("collisionobject");

        IFile paddleFile = project.getFile("logic/session/paddle.collisionobject");
        CollisionObjectNode paddle = (CollisionObjectNode) loader.load(loaderContext, "collisionobject", paddleFile.getContents());
        assertNotNull(paddle);
        Node shape = paddle.getCollisionShapeNode();
        assertNotNull(shape);
    }

}

