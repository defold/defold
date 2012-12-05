package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;

import javax.vecmath.AxisAngle4d;
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
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.Node.Flags;

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

    void assertWorld(Node node, Point3d translation, Quat4d rotation, double scale) {
        Matrix4d world = new Matrix4d();
        node.getWorldTransform(world);
        Vector3d t = new Vector3d();
        world.get(t);
        double s = world.getScale();
        Quat4d r = new Quat4d();
        world.get(r);
        double epsilon = 0.0000000001;
        assertTrue(translation.epsilonEquals(new Point3d(t.x, t.y, t.z), epsilon));
        assertTrue(rotation.epsilonEquals(r, epsilon));
        assertThat(s, is(scale));
    }

    @Test
    public void testInheritTransform() throws Exception {
        DummyNode parent = new DummyNode();
        DummyNode child = new DummyNode();
        parent.addChild(child);

        Point3d translation = new Point3d(1, 0, 0);
        Quat4d rotation = new Quat4d();
        rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), Math.PI * 0.5));
        double scale = 2.0;

        parent.setTranslation(translation);
        parent.setRotation(rotation);
        parent.setScale(scale);

        child.setTranslation(translation);
        child.setRotation(rotation);
        child.setScale(scale);

        assertThat(child.getTranslation(), is(translation));
        assertThat(child.getRotation(), is(rotation));
        assertThat(child.getScale(), is(scale));

        Point3d worldTranslation = new Point3d(1.0, 2.0, 0.0);
        Quat4d worldRotation = new Quat4d(rotation);
        worldRotation.mul(rotation);
        double worldScale = scale * scale;
        assertWorld(child, worldTranslation, worldRotation, worldScale);

    }

    @Test
    public void testNoInherit() throws Exception {
        DummyNode parent = new DummyNode();
        DummyNode childNoRotation = new DummyNode();
        childNoRotation.setFlags(Flags.NO_INHERIT_ROTATION);
        parent.addChild(childNoRotation);
        DummyNode childNoScale = new DummyNode();
        childNoScale.setFlags(Flags.NO_INHERIT_SCALE);
        parent.addChild(childNoScale);

        Point3d translation = new Point3d(1, 0, 0);
        Quat4d rotation = new Quat4d();
        rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), Math.PI * 0.5));
        double scale = 2.0;

        parent.setTranslation(translation);
        parent.setRotation(rotation);
        parent.setScale(scale);

        childNoRotation.setTranslation(translation);
        childNoRotation.setRotation(rotation);
        childNoRotation.setScale(scale);

        childNoScale.setTranslation(translation);
        childNoScale.setRotation(rotation);
        childNoScale.setScale(scale);

        Point3d worldTranslationNoRotation = new Point3d(3.0, 0.0, 0.0);
        Point3d worldTranslationNoScale = new Point3d(1.0, 1.0, 0.0);
        Quat4d worldRotation = new Quat4d(rotation);
        worldRotation.mul(rotation);
        double worldScale = scale * scale;

        assertWorld(childNoRotation, worldTranslationNoRotation, rotation, worldScale);
        assertWorld(childNoScale, worldTranslationNoScale, worldRotation, scale);
    }

    @Test
    public void testScaleAlongZ() throws Exception {
        DummyNode parent = new DummyNode();
        DummyNode childScale = new DummyScaleAlongZ(true);
        parent.addChild(childScale);
        DummyNode childNoScale = new DummyScaleAlongZ(false);
        parent.addChild(childNoScale);

        Point3d translation = new Point3d(0, 0, 1);
        Quat4d rotation = new Quat4d();
        rotation.set(new AxisAngle4d(new Vector3d(0.0, 1.0, 0.0), Math.PI * 0.5));
        double scale = 2.0;

        parent.setTranslation(translation);
        parent.setRotation(rotation);
        parent.setScale(scale);

        childScale.setTranslation(translation);
        childScale.setRotation(rotation);
        childScale.setScale(scale);

        childNoScale.setTranslation(translation);
        childNoScale.setRotation(rotation);
        childNoScale.setScale(scale);

        Point3d worldTranslationScale = new Point3d(2.0, 0.0, 1.0);
        Point3d worldTranslationNoScale = new Point3d(1.0, 0.0, 1.0);
        Quat4d worldRotation = new Quat4d(rotation);
        worldRotation.mul(rotation);
        double worldScale = scale * scale;

        assertWorld(childScale, worldTranslationScale, worldRotation, worldScale);
        assertWorld(childNoScale, worldTranslationNoScale, worldRotation, worldScale);
    }

    void assertAABBCenterX(Node node, double x) {
        AABB aabb = new AABB();
        node.getWorldAABB(aabb);
        double cx = (aabb.getMin().x + aabb.getMax().x) * 0.5;
        assertEquals(x, cx, 0.001);
    }

    @Test
    public void testAABB() throws ExecutionException {
        int c = DummyNode.AABB_EXTENTS;
        int rootX = 10;

        DummyNode r = new DummyNode();
        r.setTranslation(new Point3d(-5, 0, 0));
        assertAABBCenterX(r, -5);
        r.setTranslation(new Point3d(-rootX, 0, 0));
        assertAABBCenterX(r, -rootX);

        DummyNode c1 = new DummyNode();
        c1.setTranslation(new Point3d(1, 0, 0));
        r.addChild(c1);
        assertAABBCenterX(r, ((-rootX - c) + (-rootX + c + c)) * 0.5);

        c1.setTranslation(new Point3d(0, 0, 0));
        assertAABBCenterX(r, -rootX);

        c1.setTranslation(new Point3d(1, 0, 0));
        assertAABBCenterX(r, ((-rootX - c) + (-rootX + c + c)) * 0.5);
        r.removeChild(c1);
        assertAABBCenterX(r, -rootX);
    }

}
