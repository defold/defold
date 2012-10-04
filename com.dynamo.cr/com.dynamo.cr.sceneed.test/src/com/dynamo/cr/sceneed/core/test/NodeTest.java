package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.sceneed.core.Node;

public class NodeTest extends AbstractNodeTest {

    private DummyNode node;
    private DummyNodeLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new DummyNodeLoader();

        String extension = "dummy";

        // Mock as if dummy was a registered resource type
        IResourceType resourceType = mock(IResourceType.class);
        when(resourceType.getTemplateData()).thenReturn(new byte[0]);

        IResourceTypeRegistry registry = mock(IResourceTypeRegistry.class);
        when(registry.getResourceTypeFromExtension(extension)).thenReturn(resourceType);

        setResourceTypeRegistry(registry);

        this.node = registerAndLoadRoot(DummyNode.class, extension, this.loader);
    }

    @Test
    public void testSetProperty() throws ExecutionException {
        assertEquals(0, getNodeProperty(this.node, "dummyProperty"));
        setNodeProperty(this.node, "dummyProperty", 1);
        assertEquals(1, getNodeProperty(this.node, "dummyProperty"));
        undo();
        assertEquals(0, getNodeProperty(this.node, "dummyProperty"));
    }

    @Test
    public void testSetDynamicProperty() throws ExecutionException {
        assertEquals(0, getNodeProperty(this.node, DummyNode.DYNAMIC_PROPERTY));
        assertFalse(isNodePropertyOverridden(this.node, DummyNode.DYNAMIC_PROPERTY));
        setNodeProperty(this.node, DummyNode.DYNAMIC_PROPERTY, 1);
        assertEquals(1, getNodeProperty(this.node, DummyNode.DYNAMIC_PROPERTY));
        assertTrue(isNodePropertyOverridden(this.node, DummyNode.DYNAMIC_PROPERTY));
        undo();
        assertEquals(0, getNodeProperty(this.node, DummyNode.DYNAMIC_PROPERTY));
        assertFalse(isNodePropertyOverridden(this.node, DummyNode.DYNAMIC_PROPERTY));
    }

    @Test
    public void testBug1148() throws ExecutionException {
        Matrix4d transform = new Matrix4d();
        transform.setColumn(3, new Vector4d(10, 20, 30, 1));
        transform.set(new Quat4d(0, 1, 0, 0));
        node.setLocalTransform(transform);
        assertEquals(new Vector3d(0, 180, 0), node.getEuler());
    }

    void assertWorld(Node node, Point3d translation, Quat4d rotation) {
        Matrix4d world = new Matrix4d();
        node.getWorldTransform(world);
        Vector4d t = new Vector4d();
        world.getColumn(3, t);
        Quat4d r = new Quat4d();
        r.set(world);
        assertThat(new Point3d(t.x, t.y, t.z), is(translation));
        assertThat(r, is(rotation));
    }

    @Test
    public void testNoInheritTransform() throws Exception {
        DummyNode parent = new DummyNode();
        DummyNoInheritTransform child = new DummyNoInheritTransform();
        parent.addChild(child);

        parent.setTranslation(new Point3d(10, 0, 0));
        parent.setRotation(new Quat4d(Math.sqrt(2), 0, 0, Math.sqrt(2)));

        Point3d translation = new Point3d(1, 2, 3);
        child.setTranslation(translation);

        Quat4d rotation = new Quat4d(-0.5, -0.5, 0.5, 0.5);
        child.setRotation(rotation);

        assertThat(child.getTranslation(), is(translation));
        assertThat(child.getRotation(), is(rotation));

        assertWorld(child, translation, rotation);

        Matrix4d world = new Matrix4d();
        world.setIdentity();
        world.set(rotation);
        world.setColumn(3, new Vector4d(translation.x, translation.y, translation.z, 0));
        child.setWorldTransform(world);

        assertWorld(child, translation, rotation);
    }

}
