package com.dynamo.cr.contenteditor.scene.test;

import static org.junit.Assert.*;

import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.contenteditor.scene.CollectionLoader;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.ComponentNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.LoaderFactory;
import com.dynamo.cr.contenteditor.scene.MeshLoader;
import com.dynamo.cr.contenteditor.scene.MeshNode;
import com.dynamo.cr.contenteditor.scene.ModelLoader;
import com.dynamo.cr.contenteditor.scene.PrototypeLoader;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.Scene;

import static org.hamcrest.CoreMatchers.*;

public class CollectionTest {

    private LoaderFactory factory;
    private Scene scene;

    @Before
    public void setup() {
        factory = new TestLoaderFactory("test");
        factory.addLoader(new CollectionLoader(), "collection");
        factory.addLoader(new PrototypeLoader(), "go");
        factory.addLoader(new ModelLoader(), "model");
        factory.addLoader(new MeshLoader(), "dae");
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
        Node node = factory.load(new NullProgressMonitor(), scene, name);
        assertThat(node, instanceOf(CollectionNode.class));
        assertThat(node.getChilden().length, is(2));

        CollectionNode coll = (CollectionNode) node;
        Node[] children = coll.getChilden();

        InstanceNode attacker = getInstanceNode(children, "attacker");
        InstanceNode attacker_child1 = getInstanceNode(attacker.getChilden(), "attacker_child1");
        InstanceNode attacker_child2 = getInstanceNode(attacker.getChilden(), "attacker_child2");
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

        assertThat(attacker.getChilden().length, is(3));
        PrototypeNode attacker_prototype = (PrototypeNode) attacker.getChilden()[0];
        assertThat(attacker_prototype.getChilden().length, is(3));

        assertThat(target.getChilden().length, is(1));
        PrototypeNode target_prototype = (PrototypeNode) target.getChilden()[0];
        assertThat(target_prototype.getChilden().length, is(3));
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
        Node node = factory.load(new NullProgressMonitor(), scene, name);
        assertThat(node, instanceOf(PrototypeNode.class));
        assertThat(node.getChilden().length, is(3));

        Node[] children = node.getChilden();
        ComponentNode n1 = getComponentNode(children, "attacker.script");
        ComponentNode n2 = getComponentNode(children, "attacker.collisionobject");
        ComponentNode n3 = getComponentNode(children, "box.model");

        assertThat(n1.getParent(), equalTo((Node) node));
        assertThat(n2.getParent(), equalTo((Node) node));
        assertThat(n3.getParent(), equalTo((Node) node));

        assertThat(node.contains(n1), is(true));
        assertThat(node.contains(n2), is(true));
        assertThat(node.contains(n3), is(true));

        assertThat(n3.getChilden().length, is(1));
        Node mn = n3.getChilden()[0];
        assertThat(mn, instanceOf(MeshNode.class));
    }
}

