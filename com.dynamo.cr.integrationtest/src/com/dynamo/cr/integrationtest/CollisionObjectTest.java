package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertNotNull;

import org.eclipse.core.resources.IFile;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.sceneed.core.Node;

public class CollisionObjectTest extends AbstractNodeTest {

    @Test
    public void testCollisionObject() throws Exception {
        IFile coFile = project.getFile("collision_object/kinematic_box.collisionobject");
        CollisionObjectNode co = (CollisionObjectNode) this.loaderContext.loadNode("collisionobject", coFile.getContents());
        assertNotNull(co);
        Node shape = co.getCollisionShapeNode();
        assertNotNull(shape);
    }

}

