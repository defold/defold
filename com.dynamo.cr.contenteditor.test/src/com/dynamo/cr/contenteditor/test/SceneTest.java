package com.dynamo.cr.contenteditor.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;

import javax.vecmath.Matrix4d;

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
import com.dynamo.cr.contenteditor.scene.CameraNode;
import com.dynamo.cr.contenteditor.scene.CameraNodeLoader;
import com.dynamo.cr.contenteditor.scene.CollectionInstanceNode;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.CollectionNodeLoader;
import com.dynamo.cr.contenteditor.scene.CollisionNode;
import com.dynamo.cr.contenteditor.scene.CollisionNodeLoader;
import com.dynamo.cr.contenteditor.scene.ComponentNode;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.LightNode;
import com.dynamo.cr.contenteditor.scene.LightNodeLoader;
import com.dynamo.cr.contenteditor.scene.MeshNode;
import com.dynamo.cr.contenteditor.scene.MeshNodeLoader;
import com.dynamo.cr.contenteditor.scene.ModelNode;
import com.dynamo.cr.contenteditor.scene.ModelNodeLoader;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.PrototypeNodeLoader;
import com.dynamo.cr.contenteditor.scene.Scene;
import com.dynamo.cr.contenteditor.scene.SpriteNode;
import com.dynamo.cr.contenteditor.scene.SpriteNodeLoader;

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
        factory.addLoader(new CameraNodeLoader(), "camera");
        factory.addLoader(new LightNodeLoader(), "light");
        factory.addLoader(new SpriteNodeLoader(), "sprite");
        factory.addLoader(new CollisionNodeLoader(), "collisionobject");
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
        ComponentNode n1 = getComponentNode(children, "empty.script");
        ComponentNode n2 = getComponentNode(children, "test.collisionobject");
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
                assertThat(node.getFlags() & Node.FLAG_CAN_HAVE_CHILDREN, not(0));
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
        assertThat(node.getFlags(), is(Node.FLAG_EDITABLE | Node.FLAG_CAN_HAVE_CHILDREN));
        int flags = Node.FLAG_EDITABLE | Node.FLAG_TRANSFORMABLE;
        for (Node child : node.getChildren()) {
            assertThat((child.getFlags() & flags), is(flags));
            if (child instanceof InstanceNode) {
                assertThat(child.getFlags() & Node.FLAG_CAN_HAVE_CHILDREN, not(0));
                testNodeFlagsExcludeInstance(child.getChildren(), flags);
            } else {
                assertThat(child.getFlags() & Node.FLAG_CAN_HAVE_CHILDREN, is(0));
                testNodeFlags(child.getChildren(), flags);
            }
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
        root.getChildren()[0].getChildren()[0].setFlags(root.getChildren()[0].getChildren()[0].getFlags() | Node.FLAG_CAN_HAVE_CHILDREN);
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

        // Test id uniqueness of collection instances
        assertTrue(root.isChildIdentifierUsed(root.getChildren()[0], root.getChildren()[0].getIdentifier()));
        assertTrue(!root.isChildIdentifierUsed(root.getChildren()[0], root.getUniqueChildIdentifier(root.getChildren()[0])));
        // Test id uniqueness of instances
        assertTrue(root.isChildIdentifierUsed(root.getChildren()[4], root.getChildren()[4].getIdentifier()));
        assertTrue(!root.isChildIdentifierUsed(root.getChildren()[4], root.getUniqueChildIdentifier(root.getChildren()[4])));
        // Instances should also support unique ids given their ancestor CollectioNode
        assertTrue(root.getChildren()[4].isChildIdentifierUsed(root.getChildren()[4], root.getChildren()[4].getIdentifier()));
        assertTrue(!root.getChildren()[4].isChildIdentifierUsed(root.getChildren()[4], root.getChildren()[4].getUniqueChildIdentifier(root.getChildren()[4])));
    }

    @Test
    public void testRecursion() throws Exception {
        String collectionName = "recurse.collection";
        Node parent = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        assertThat(parent, instanceOf(CollectionNode.class));
        assertThat(parent.getChildren().length, is(4));

        // Collection instances
        assertTrue(parent.getChildren()[0] instanceof BrokenNode);
        assertTrue(parent.getChildren()[1] instanceof CollectionInstanceNode);
        assertTrue(parent.getChildren()[1].getChildren()[0] instanceof CollectionNode);
        assertTrue(parent.getChildren()[1].getChildren()[0].getChildren()[0] instanceof BrokenNode);

        // Instances
        assertTrue(parent.getChildren()[2] instanceof InstanceNode);
        assertTrue(parent.getChildren()[2].getChildren()[1] instanceof BrokenNode);
        assertTrue(parent.getChildren()[3] instanceof InstanceNode);
        assertTrue(parent.getChildren()[3].getChildren()[1] instanceof InstanceNode);
        assertTrue(parent.getChildren()[3].getChildren()[1].getChildren()[1] instanceof BrokenNode);
    }

    /**
     * This compares the sub tree starting with this node as root, that is it does not compare the parent, scene or pseudo state.
     */
    public void deepCompare(Node a, Node b) throws Exception {
        Matrix4d mA = new Matrix4d();
        a.getLocalTransform(mA);
        Matrix4d mB = new Matrix4d();
        b.getLocalTransform(mB);
        String idA = a.getIdentifier();
        String idB = b.getIdentifier();
        assertTrue(mA.equals(mB) && a.getFlags() == b.getFlags() && (idA == null ? idB == null : idB.equals(idB)));
        assertTrue(a.getChildren().length == b.getChildren().length);
        for (int i = 0; i < a.getChildren().length; ++i) {
            deepCompare(a.getChildren()[i], b.getChildren()[i]);
        }
    }

    @Test
    public void testSaveAndLoad() throws Exception {
        String collectionName = "test.collection";
        String tmpName = "tmp.collection";
        Node root = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        this.factory.save(new NullProgressMonitor(), tmpName, root, out);
        FileOutputStream fileOut = new FileOutputStream("test/" + tmpName);
        fileOut.write(out.toByteArray());
        fileOut.close();
        Scene tmpScene = new Scene();
        Node tmpRoot = this.factory.load(new NullProgressMonitor(), tmpScene, tmpName, null);
        assertTrue(tmpRoot != null);
        deepCompare(root, tmpRoot);
        File tmpFile = new File("test/" + tmpName);
        assertTrue(tmpFile.delete());

        // Save not possible for collections containing errors
        collectionName = "dup_id.collection";
        root = this.factory.load(new NullProgressMonitor(), this.scene, collectionName, null);
        assertTrue(root != null);
        assertTrue(!root.isOk());
        out = new ByteArrayOutputStream();
        boolean saved = true;
        try {
            this.factory.save(new NullProgressMonitor(), tmpName, root, out);
        } catch (Exception e) {
            saved = false;
        }
        assertTrue(!saved);
    }

    @Test
    public void testHierarchy() throws Exception {
        final int TYPE_BROKEN = 0;
        final int TYPE_CAMERA = 1;
        final int TYPE_COLLECTION_INSTANCE = 2;
        final int TYPE_COLLECTION = 3;
        final int TYPE_COLLISION = 4;
        final int TYPE_INSTANCE = 5;
        final int TYPE_LIGHT = 6;
        final int TYPE_MESH = 7;
        final int TYPE_MODEL = 8;
        final int TYPE_PROTOTYPE = 9;
        final int TYPE_SPRITE = 10;
        final int TYPE_COUNT = 11;

        Node[][] nodes = new Node[TYPE_COUNT][2];

        nodes[TYPE_BROKEN][0] = new BrokenNode(this.scene, "broken0", null);
        nodes[TYPE_BROKEN][1] = new BrokenNode(this.scene, "broken1", null);
        nodes[TYPE_CAMERA][0] = new CameraNode(this.scene, "camera0", null);
        nodes[TYPE_CAMERA][1] = new CameraNode(this.scene, "camera1", null);
        nodes[TYPE_COLLECTION_INSTANCE][0] = new CollectionInstanceNode(this.scene, "collection_instance0", null, new CollectionNode(this.scene, "collection2", null));
        nodes[TYPE_COLLECTION_INSTANCE][1] = new CollectionInstanceNode(this.scene, "collection_instance1", null, new CollectionNode(this.scene, "collection3", null));
        nodes[TYPE_COLLECTION][0] = new CollectionNode(this.scene, "collection0", null);
        nodes[TYPE_COLLECTION][1] = new CollectionNode(this.scene, "collection1", null);
        nodes[TYPE_COLLISION][0] = new CollisionNode(this.scene, "collision0", null, null);
        nodes[TYPE_COLLISION][1] = new CollisionNode(this.scene, "collision1", null, null);
        nodes[TYPE_INSTANCE][0] = new InstanceNode(this.scene, "instance0", null, new PrototypeNode(this.scene, "prototype2"));
        nodes[TYPE_INSTANCE][1] = new InstanceNode(this.scene, "instance1", null, new PrototypeNode(this.scene, "prototype3"));
        nodes[TYPE_LIGHT][0] = new LightNode(this.scene, null, null);
        nodes[TYPE_LIGHT][1] = new LightNode(this.scene, null, null);
        nodes[TYPE_MESH][0] = new MeshNode(this.scene, "mesh0", null);
        nodes[TYPE_MESH][1] = new MeshNode(this.scene, "mesh1", null);
        nodes[TYPE_MODEL][0] = new ModelNode(this.scene, "model0", new MeshNode(this.scene, "mesh2", null));
        nodes[TYPE_MODEL][1] = new ModelNode(this.scene, "model1", new MeshNode(this.scene, "mesh3", null));
        nodes[TYPE_PROTOTYPE][0] = new PrototypeNode(this.scene, "prototype0");
        nodes[TYPE_PROTOTYPE][1] = new PrototypeNode(this.scene, "prototype1");
        nodes[TYPE_SPRITE][0] = new SpriteNode(this.scene, "sprite0", null, null);
        nodes[TYPE_SPRITE][1] = new SpriteNode(this.scene, "sprite1", null, null);

        for (int i = 0; i < TYPE_COUNT; ++i) {
            assertTrue(nodes[i][0] != null);
            assertTrue(nodes[i][1] != null);
        }

        boolean[][] accepts = new boolean[TYPE_COUNT][TYPE_COUNT];
        for (int i = 0; i < TYPE_COUNT; ++i) {
            for (int j = 0; j < TYPE_COUNT; ++j) {
                accepts[i][j] = false;
            }
        }

        accepts[TYPE_COLLECTION][TYPE_BROKEN] = true;
        accepts[TYPE_COLLECTION][TYPE_COLLECTION_INSTANCE] = true;
        accepts[TYPE_COLLECTION][TYPE_INSTANCE] = true;

        accepts[TYPE_COLLECTION_INSTANCE][TYPE_COLLECTION] = true;

        accepts[TYPE_PROTOTYPE][TYPE_BROKEN] = true;
        accepts[TYPE_PROTOTYPE][TYPE_CAMERA] = true;
        accepts[TYPE_PROTOTYPE][TYPE_COLLISION] = true;
        accepts[TYPE_PROTOTYPE][TYPE_LIGHT] = true;
        accepts[TYPE_PROTOTYPE][TYPE_MODEL] = true;
        accepts[TYPE_PROTOTYPE][TYPE_SPRITE] = true;

        accepts[TYPE_INSTANCE][TYPE_BROKEN] = true;
        accepts[TYPE_INSTANCE][TYPE_INSTANCE] = true;
        accepts[TYPE_INSTANCE][TYPE_PROTOTYPE] = true;

        accepts[TYPE_MODEL][TYPE_MESH] = true;

        for (int i = 0; i < TYPE_COUNT; ++i) {
            for (int j = 0; j < TYPE_COUNT; ++j) {
                assertThat(nodes[i][0].acceptsChild(nodes[j][1]), equalTo(accepts[i][j]));
                try {
                    nodes[i][0].addNode(nodes[j][1]);
                    nodes[i][0].removeNode(nodes[j][1]);
                    assertTrue(accepts[i][j]);
                } catch (Exception e) {
                    assertTrue(!accepts[i][j]);
                }
            }
        }
    }
}

