package com.dynamo.cr.scene.test.graph;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.CharArrayWriter;
import java.io.IOException;
import java.util.ArrayList;

import javax.vecmath.Matrix4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.scene.graph.CameraNode;
import com.dynamo.cr.scene.graph.CollectionInstanceNode;
import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.CollisionNode;
import com.dynamo.cr.scene.graph.ComponentNode;
import com.dynamo.cr.scene.graph.InstanceNode;
import com.dynamo.cr.scene.graph.LightNode;
import com.dynamo.cr.scene.graph.ModelNode;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.NodeFactory;
import com.dynamo.cr.scene.graph.PrototypeNode;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.graph.SpriteNode;
import com.dynamo.cr.scene.resource.CameraResource;
import com.dynamo.cr.scene.resource.CollectionResource;
import com.dynamo.cr.scene.resource.CollisionResource;
import com.dynamo.cr.scene.resource.ConvexShapeResource;
import com.dynamo.cr.scene.resource.LightResource;
import com.dynamo.cr.scene.resource.MeshResource;
import com.dynamo.cr.scene.resource.ModelResource;
import com.dynamo.cr.scene.resource.PrototypeResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.resource.ResourceFactory;
import com.dynamo.cr.scene.resource.SpriteResource;
import com.dynamo.cr.scene.resource.TextureResource;
import com.dynamo.cr.scene.test.util.SceneContext;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.google.protobuf.TextFormat;

public class SceneTest {

    private NodeFactory nodeFactory;
    private ResourceFactory resourceFactory;
    private Scene scene;
    private IProject project;

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
        this.project = context.project;
    }

    InstanceNode getInstanceNode(Node[] nodes, String resource) {
        for (Node n : nodes) {
            if (n instanceof InstanceNode) {
                InstanceNode in = (InstanceNode) n;
                if (in.getIdentifier().equals(resource)) {
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

        Resource resource = resourceFactory.load(new NullProgressMonitor(), name);
        Node node = nodeFactory.create(name, resource, null, scene);
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
        Resource resource = resourceFactory.load(new NullProgressMonitor(), name);
        Node node = nodeFactory.create(name, resource, null, scene);
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

        assertThat(n3.getChildren().length, is(0));
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
        Resource resource = resourceFactory.load(new NullProgressMonitor(), name);
        Node node = nodeFactory.create(name, resource, null, scene);
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
        Resource resource = resourceFactory.load(new NullProgressMonitor(), collectionName);
        Node root = nodeFactory.create(collectionName, resource, null, scene);
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
        Resource resource = resourceFactory.load(new NullProgressMonitor(), collectionName);
        Node parent = nodeFactory.create(collectionName, resource, null, scene);
        assertThat(parent, instanceOf(CollectionNode.class));
        assertThat(parent.getChildren().length, is(4));

        // Collection instances
        assertTrue(parent.getChildren()[0] instanceof CollectionInstanceNode);
        assertTrue(parent.getChildren()[0].hasError(Node.ERROR_FLAG_RECURSION));
        assertTrue(parent.getChildren()[1] instanceof CollectionInstanceNode);
        assertTrue(parent.getChildren()[1].getChildren()[0] instanceof CollectionNode);
        assertTrue(parent.getChildren()[1].getChildren()[0].getChildren()[0] instanceof CollectionInstanceNode);
        assertTrue(parent.getChildren()[1].getChildren()[0].getChildren()[0].hasError(Node.ERROR_FLAG_RECURSION));

        // Instances
        assertTrue(parent.getChildren()[2] instanceof InstanceNode);
        assertTrue(parent.getChildren()[2].getChildren()[1] instanceof InstanceNode);
        assertTrue(parent.getChildren()[2].getChildren()[1].hasError(Node.ERROR_FLAG_RECURSION));
        assertTrue(parent.getChildren()[3] instanceof InstanceNode);
        assertTrue(parent.getChildren()[3].getChildren()[1] instanceof InstanceNode);
        assertTrue(parent.getChildren()[3].getChildren()[1].getChildren()[1] instanceof InstanceNode);
        assertTrue(parent.getChildren()[3].getChildren()[1].getChildren()[1].hasError(Node.ERROR_FLAG_RECURSION));

        // Check that removing the nodes fixes the problem
        assertTrue(parent.hasError(Node.ERROR_FLAG_CHILD_ERROR));
        parent.removeNode(parent.getChildren()[0]);
        parent.removeNode(parent.getChildren()[0]);
        parent.getChildren()[0].removeNode(parent.getChildren()[0].getChildren()[1]);
        parent.getChildren()[1].getChildren()[1].removeNode(parent.getChildren()[1].getChildren()[1].getChildren()[1]);
        assertTrue(!parent.hasError(Node.ERROR_FLAG_CHILD_ERROR));
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
        Resource resource = resourceFactory.load(new NullProgressMonitor(), collectionName);
        CollectionNode root = (CollectionNode)nodeFactory.create(collectionName, resource, null, scene);
        CollectionDesc desc = root.buildDescriptor();
        IFile tmpFile = this.project.getFile(tmpName);
        CharArrayWriter output = new CharArrayWriter();
        TextFormat.print(desc, output);
        output.close();
        tmpFile.create(new ByteArrayInputStream(output.toString().getBytes()), 0, new NullProgressMonitor());
        assertTrue(tmpFile.exists());
        Scene tmpScene = new Scene();
        Resource tmpResource = resourceFactory.load(new NullProgressMonitor(), tmpName);
        Node tmpRoot = nodeFactory.create(tmpName, tmpResource, null, tmpScene);
        assertTrue(tmpRoot != null);
        deepCompare(root, tmpRoot);
        tmpFile.delete(true, new NullProgressMonitor());
        assertFalse(tmpFile.exists());
    }

    @Test
    public void testHierarchy() throws Exception {
        final int TYPE_CAMERA = 0;
        final int TYPE_COLLECTION_INSTANCE = 1;
        final int TYPE_COLLECTION = 2;
        final int TYPE_COLLISION = 3;
        final int TYPE_INSTANCE = 4;
        final int TYPE_LIGHT = 5;
        final int TYPE_MODEL = 6;
        final int TYPE_PROTOTYPE = 7;
        final int TYPE_SPRITE = 8;
        final int TYPE_COUNT = 9;

        Node[][] nodes = new Node[TYPE_COUNT][2];

        nodes[TYPE_CAMERA][0] = new CameraNode("camera0", new CameraResource("", null), this.scene);
        nodes[TYPE_CAMERA][1] = new CameraNode("camera1", new CameraResource("", null), this.scene);
        nodes[TYPE_COLLECTION_INSTANCE][0] = new CollectionInstanceNode("collection_instance0", this.scene, null, new CollectionNode("collection2", new CollectionResource("", null), this.scene, this.nodeFactory));
        nodes[TYPE_COLLECTION_INSTANCE][1] = new CollectionInstanceNode("collection_instance1", this.scene, null, new CollectionNode("collection3", new CollectionResource("", null), this.scene, this.nodeFactory));
        nodes[TYPE_COLLECTION][0] = new CollectionNode("collection0", new CollectionResource("", null), this.scene, nodeFactory);
        nodes[TYPE_COLLECTION][1] = new CollectionNode("collection1", new CollectionResource("", null), this.scene, nodeFactory);
        nodes[TYPE_COLLISION][0] = new CollisionNode("collision0", new CollisionResource("", null, new ConvexShapeResource("", null)), this.scene);
        nodes[TYPE_COLLISION][1] = new CollisionNode("collision1", new CollisionResource("", null, new ConvexShapeResource("", null)), this.scene);
        nodes[TYPE_INSTANCE][0] = new InstanceNode("instance0", this.scene, null, new PrototypeNode("prototype2", new PrototypeResource("", null, new ArrayList<Resource>()), this.scene, this.nodeFactory));
        nodes[TYPE_INSTANCE][1] = new InstanceNode("instance1", this.scene, null, new PrototypeNode("prototype3", new PrototypeResource("", null, new ArrayList<Resource>()), this.scene, this.nodeFactory));
        nodes[TYPE_LIGHT][0] = new LightNode("light0", new LightResource("", null), this.scene);
        nodes[TYPE_LIGHT][1] = new LightNode("light1", new LightResource("", null), this.scene);
        nodes[TYPE_MODEL][0] = new ModelNode("model0", new ModelResource("", null, new MeshResource("", null), new ArrayList<TextureResource>()), this.scene);
        nodes[TYPE_MODEL][1] = new ModelNode("model1", new ModelResource("", null, new MeshResource("", null), new ArrayList<TextureResource>()), this.scene);
        nodes[TYPE_PROTOTYPE][0] = new PrototypeNode("prototype0", new PrototypeResource("", null, new ArrayList<Resource>()), this.scene, this.nodeFactory);
        nodes[TYPE_PROTOTYPE][1] = new PrototypeNode("prototype1", new PrototypeResource("", null, new ArrayList<Resource>()), this.scene, this.nodeFactory);
        nodes[TYPE_SPRITE][0] = new SpriteNode("sprite0", new SpriteResource("", null, new TextureResource("", new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR))), this.scene);
        nodes[TYPE_SPRITE][1] = new SpriteNode("sprite1", new SpriteResource("", null, new TextureResource("", new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR))), this.scene);

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

        accepts[TYPE_COLLECTION][TYPE_COLLECTION_INSTANCE] = true;
        accepts[TYPE_COLLECTION][TYPE_INSTANCE] = true;

        accepts[TYPE_COLLECTION_INSTANCE][TYPE_COLLECTION] = true;

        accepts[TYPE_PROTOTYPE][TYPE_CAMERA] = true;
        accepts[TYPE_PROTOTYPE][TYPE_COLLISION] = true;
        accepts[TYPE_PROTOTYPE][TYPE_LIGHT] = true;
        accepts[TYPE_PROTOTYPE][TYPE_MODEL] = true;
        accepts[TYPE_PROTOTYPE][TYPE_SPRITE] = true;

        accepts[TYPE_INSTANCE][TYPE_INSTANCE] = true;
        accepts[TYPE_INSTANCE][TYPE_PROTOTYPE] = true;

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

