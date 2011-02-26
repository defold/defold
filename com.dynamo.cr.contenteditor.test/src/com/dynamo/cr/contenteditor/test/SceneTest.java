package com.dynamo.cr.contenteditor.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.contenteditor.resource.CameraLoader;
import com.dynamo.cr.contenteditor.resource.CollisionLoader;
import com.dynamo.cr.contenteditor.resource.ConvexShapeLoader;
import com.dynamo.cr.contenteditor.resource.LightLoader;
import com.dynamo.cr.contenteditor.resource.SpriteLoader;
import com.dynamo.cr.contenteditor.resource.TextureLoader;
import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.BrokenNode;
import com.dynamo.cr.contenteditor.scene.CollectionInstanceNode;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.CollectionNodeLoader;
import com.dynamo.cr.contenteditor.scene.ComponentNode;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.MeshNode;
import com.dynamo.cr.contenteditor.scene.MeshNodeLoader;
import com.dynamo.cr.contenteditor.scene.ModelNodeLoader;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.PrototypeNodeLoader;
import com.dynamo.cr.contenteditor.scene.Scene;

public class SceneTest {

    private AbstractNodeLoaderFactory factory;
    private FileResourceLoaderFactory resourceFactory;
    private Scene scene;

    @Before
    public void setup() {
        String root = "test";
        resourceFactory = new FileResourceLoaderFactory(root);
        resourceFactory.addLoader(new TextureLoader(), "png");
        resourceFactory.addLoader(new CameraLoader(), "camera");
        resourceFactory.addLoader(new LightLoader(), "light");
        resourceFactory.addLoader(new SpriteLoader(), "sprite");
        resourceFactory.addLoader(new CollisionLoader(), "collisionobject");
        resourceFactory.addLoader(new ConvexShapeLoader(), "convexshape");
        factory = new FileNodeLoaderFactory(root, resourceFactory);
        factory.addLoader(new CollectionNodeLoader(), "collection");
        factory.addLoader(new PrototypeNodeLoader(), "go");
        factory.addLoader(new ModelNodeLoader(), "model");
        factory.addLoader(new MeshNodeLoader(), "dae");
        scene = new Scene();
    }

    InstanceNode getInstanceNode(Node[] nodes, String resource) {
        for (Node n : nodes) {
            if (n instanceof InstanceNode) {
                InstanceNode in = (InstanceNode) n;
                if (in.getName().equals(resource)) {
                    return in;
                }
            }
        }
        assertTrue(false);
        return null;
    }

    @Test
    public void testCollectionLoader() throws Exception {
        String name = "test.collection";
        Node node = factory.load(new NullProgressMonitor(), scene, name, null);
        assertThat(node, instanceOf(CollectionNode.class));
        assertThat(node.getChildren().length, is(3));

        CollectionNode coll = (CollectionNode) node;
        Node[] children = coll.getChildren();

        InstanceNode attacker = getInstanceNode(children, "attacker");
        InstanceNode attacker_child1 = getInstanceNode(attacker.getChildren(), "attacker_child1");
        InstanceNode attacker_child2 = getInstanceNode(attacker.getChildren(), "attacker_child2");
        InstanceNode target = getInstanceNode(children, "target");

        assertThat(attacker.getPrototype(), is("attacker.go"));
        assertThat(attacker_child1.getPrototype(), is("attacker_child.go"));
        assertThat(attacker_child2.getPrototype(), is("attacker_child.go"));
        assertThat(target.getPrototype(), is("target.go"));

        assertThat(attacker.getParent(), equalTo((Node) coll));

        // TODO: MAKE WORK
        assertThat(attacker_child1.getParent(), equalTo((Node) attacker));
        assertThat(attacker_child2.getParent(), equalTo((Node) attacker));
        assertThat(target.getParent(), equalTo((Node) coll));

        assertThat(node.contains(attacker), is(true));
        assertThat(node.contains(target), is(true));

        assertThat(attacker.getChildren().length, is(3));
        PrototypeNode attacker_prototype = (PrototypeNode) attacker.getChildren()[0];
        assertThat(attacker_prototype.getChildren().length, is(3));

        assertThat(target.getChildren().length, is(1));
        PrototypeNode target_prototype = (PrototypeNode) target.getChildren()[0];
        assertThat(target_prototype.getChildren().length, is(3));
    }

    ComponentNode getComponentNode(Node[] nodes, String resource) {
        for (Node n : nodes) {
            ComponentNode cn = (ComponentNode) n;
            if (cn.getResource().equals(resource)) {
                return cn;
            }
        }
        assertTrue(false);
        return null;
    }

    @Test
    public void testPrototypeLoader() throws Exception {
        String name = "attacker.go";
        Node node = factory.load(new NullProgressMonitor(), scene, name, null);
        assertThat(node, instanceOf(PrototypeNode.class));
        assertThat(node.getChildren().length, is(3));

        Node[] children = node.getChildren();
        ComponentNode n1 = getComponentNode(children, "attacker.script");
        ComponentNode n2 = getComponentNode(children, "attacker.collisionobject");
        ComponentNode n3 = getComponentNode(children, "box.model");

        assertThat(n1.getParent(), equalTo((Node) node));
        assertThat(n2.getParent(), equalTo((Node) node));
        assertThat(n3.getParent(), equalTo((Node) node));

        assertThat(node.contains(n1), is(true));
        assertThat(node.contains(n2), is(true));
        assertThat(node.contains(n3), is(true));

        assertThat(n3.getChildren().length, is(1));
        Node mn = n3.getChildren()[0];
        assertThat(mn, instanceOf(MeshNode.class));
    }

    private void testNodeFlags(Node[] nodes, int flags) throws Exception {
        for (Node node : nodes) {
            assertThat((node.getFlags() & flags), is(0));
            testNodeFlags(node.getChildren(), flags);
        }
    }

    private void testNodeFlagsExcludeInstance(Node[] nodes, int flags) throws Exception {
        for (Node node : nodes) {
            if (!(node instanceof InstanceNode)) {
                assertThat((node.getFlags() & flags), is(0));
                testNodeFlags(node.getChildren(), flags);
            } else {
                testNodeFlagsExcludeInstance(node.getChildren(), flags);
            }
        }
    }

    @Test
    public void testNodeFlags() throws Exception {
        String name = "test.collection";
        Node node = factory.load(new NullProgressMonitor(), scene, name, null);
        assertThat(node, instanceOf(CollectionNode.class));
        assertThat(node.getChildren().length, is(3));
        assertThat(node.getFlags(), is(Node.FLAG_CAN_HAVE_CHILDREN));
        int flags = Node.FLAG_LABEL_EDITABLE
            | Node.FLAG_SELECTABLE
            | Node.FLAG_TRANSFORMABLE;
        for (Node child : node.getChildren()) {
            assertThat((child.getFlags() & flags), is(flags));
            if (child instanceof InstanceNode)
                testNodeFlagsExcludeInstance(child.getChildren(), flags);
            else
                testNodeFlags(child.getChildren(), flags);
        }
    }

    @Test
    public void testIds() throws Exception {
        String collectionName = "dup_id.collection";
        Node root = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        assertThat(root, instanceOf(CollectionNode.class));
        assertThat(root.getChildren().length, is(6));

        // Duplicated ids in instances of collection instance
        assertTrue(root.getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(root.getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertEquals("id", root.getChildren()[0].getChildren()[0].getChildren()[0].getIdentifier());
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("id", root.getChildren()[0].getChildren()[0].getChildren()[1].getIdentifier());
        assertTrue(root.getChildren()[0].getChildren()[0].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Set id to clear error status
        root.getChildren()[0].getChildren()[0].getChildren()[0].setIdentifier("id2");
        assertEquals("id2", root.getChildren()[0].getChildren()[0].getChildren()[0].getIdentifier());
        assertTrue(!root.getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Reset id to set error status
        root.getChildren()[0].getChildren()[0].getChildren()[0].setIdentifier("id");
        assertEquals("id", root.getChildren()[0].getChildren()[0].getChildren()[0].getIdentifier());
        assertTrue(root.getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(root.getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(root.getChildren()[0].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Remove node to clear error status
        Node node = root.getChildren()[0].getChildren()[0].getChildren()[0];
        root.getChildren()[0].getChildren()[0].removeNode(node);
        assertTrue(!root.getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Add node to set error status
        root.getChildren()[0].getChildren()[0].addNode(node);
        assertTrue(root.getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(root.getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[0].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[0].getChildren()[0].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));

        // Duplicated ids in collection instances
        assertTrue(!root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("id", root.getChildren()[1].getIdentifier());
        assertTrue(root.getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("id", root.getChildren()[2].getIdentifier());
        // Set id to clear error status
        root.getChildren()[1].setIdentifier("id2");
        assertEquals("id2", root.getChildren()[1].getIdentifier());
        assertTrue(!root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Reset id to set error status
        root.getChildren()[1].setIdentifier("id");
        assertEquals("id", root.getChildren()[1].getIdentifier());
        assertTrue(root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));

        // Duplicated ids in instances
        assertTrue(!root.getChildren()[3].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("id", root.getChildren()[3].getIdentifier());
        assertTrue(root.getChildren()[4].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("id", root.getChildren()[4].getIdentifier());
        // Clearing/setting error for InstanceNodes is already tested above

        // Duplicated child ids in instance
        assertTrue(!root.getChildren()[5].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertEquals("child", root.getChildren()[5].getChildren()[1].getIdentifier());
        assertTrue(!root.getChildren()[5].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertEquals("child2", root.getChildren()[5].getChildren()[2].getIdentifier());
        assertTrue(!root.getChildren()[5].getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Set id to set error status
        root.getChildren()[5].getChildren()[1].setIdentifier("child2");
        assertTrue(root.getChildren()[5].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(root.getChildren()[5].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[5].getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Remove node to clear error status
        node = root.getChildren()[5].getChildren()[1];
        root.getChildren()[5].removeNode(node);
        assertTrue(!root.getChildren()[5].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[5].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Add node to set error status
        root.getChildren()[5].addNode(node);
        assertTrue(root.getChildren()[5].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[5].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[5].getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Reset id to clear error status
        root.getChildren()[5].getChildren()[2].setIdentifier("child");
        assertTrue(!root.getChildren()[5].hasError(Node.ERROR_FLAG_CHILD_ERROR));
        assertTrue(!root.getChildren()[5].getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(!root.getChildren()[5].getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));

        // Remove collection instance to clear error status (put last since it messes up indices)
        node = root.getChildren()[1];
        root.removeNode(node);
        assertTrue(!root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        // Add node to set error status
        root.addNode(node);
        assertTrue(!root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[5] instanceof CollectionInstanceNode);
        assertTrue(root.getChildren()[5].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
    }

    @Test
    public void testCollidingIdsAtLoading() throws Exception {
        String collectionName = "colliding_ids.collection";
        Node root = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        assertThat(root, instanceOf(CollectionNode.class));
        assertThat(root.getChildren().length, is(4));
        assertTrue(root.getChildren()[0] instanceof CollectionInstanceNode);
        assertTrue(!root.getChildren()[0].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[1] instanceof CollectionInstanceNode);
        assertTrue(root.getChildren()[1].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[2] instanceof InstanceNode);
        assertTrue(!root.getChildren()[2].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
        assertTrue(root.getChildren()[3] instanceof InstanceNode);
        assertTrue(root.getChildren()[3].hasError(Node.ERROR_FLAG_DUPLICATE_ID));
    }

    @Test
    public void testRecursion() throws Exception {
        String collectionName = "recurse.collection";
        Node parent = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        assertThat(parent, instanceOf(CollectionNode.class));
        assertThat(parent.getChildren().length, is(2));
        assertTrue(parent.getChildren()[0] instanceof BrokenNode);
        assertTrue(parent.getChildren()[1] instanceof CollectionInstanceNode);
        assertTrue(parent.getChildren()[1].getChildren()[0] instanceof CollectionNode);
        assertTrue(parent.getChildren()[1].getChildren()[0].getChildren()[0] instanceof BrokenNode);
    }
}

