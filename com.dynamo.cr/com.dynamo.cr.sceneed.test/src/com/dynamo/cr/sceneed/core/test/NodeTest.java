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
import javax.vecmath.Matrix3d;
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

    void assertWorld(Node node, Point3d translation, Quat4d rotation, Vector3d scale) {
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
        assertThat(new Vector3d(s, s, s), is(scale));
    }

    @Test
    public void testInheritTransform() throws Exception {
        DummyNode parent = new DummyNode();
        DummyNode child = new DummyNode();
        parent.addChild(child);

        Point3d translation = new Point3d(1, 0, 0);
        Quat4d rotation = new Quat4d();
        rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), Math.PI * 0.5));
        Vector3d scale = new Vector3d(2.0, 2.0, 2.0);

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
        Vector3d worldScale = new Vector3d(scale.x * scale.x, scale.y * scale.y, scale.z * scale.z);
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
        Vector3d scale = new Vector3d(2.0, 2.0, 2.0);

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
        Vector3d worldScale = new Vector3d(scale.x * scale.x, scale.y * scale.y, scale.z * scale.z);

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
        Vector3d scale = new Vector3d(2.0, 2.0, 2.0);

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
        Vector3d worldScale = new Vector3d(scale.x * scale.x, scale.y * scale.y, scale.z * scale.z);

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

    @Test
    public void testNodeTransform() throws Exception {
        DummyNode node = new DummyNode();
        Matrix4d tmpMat = new Matrix4d();
        Matrix4d refMat = new Matrix4d();
        refMat.setIdentity();
        Matrix4d transform = new Matrix4d();
        final double epsilon = 0.0000000001;

        node.setTranslation(new Point3d(0.25, 0.5, 0.75));
        node.getWorldTransform(transform);
        refMat.setTranslation(new Vector3d(0.25, 0.5, 0.75));
        assertTrue(transform.epsilonEquals(refMat, epsilon));

        node.setEuler(new Vector3d(90.0*0.25, 90.0*0.5, 90.0));
        node.getWorldTransform(transform);
        double radians = 90.0 * Math.PI / 180.0;
        tmpMat.setIdentity();

        // Rotation sequence 231 (YZX)
        tmpMat.setRotation(new AxisAngle4d(new Vector3d(0.0, 1.0, 0.0), radians*0.5));
        refMat.mul(tmpMat);
        tmpMat.setRotation(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), radians));
        refMat.mul(tmpMat);
        tmpMat.setRotation(new AxisAngle4d(new Vector3d(1.0, 0.0, 0.0), radians*0.25));
        refMat.mul(tmpMat);
        assertTrue(transform.epsilonEquals(refMat, epsilon));

        node.setComponentScale(new Vector3d(0.25, 0.5, 0.75));
        node.getWorldTransform(transform);
        tmpMat.setIdentity();
        tmpMat.setElement(0, 0, 0.25);
        tmpMat.setElement(1, 1, 0.5);
        tmpMat.setElement(2, 2, 0.75);
        refMat.mul(tmpMat);
        assertTrue(transform.epsilonEquals(refMat, epsilon));
    }

    @Test
    public void testNodeQuatToEuler() throws Exception {
        final double epsilon = 0.02;
        Vector3d euler = new Vector3d();

        // different degrees around different single axis
        final double halfRadFactor = Math.PI / 360.0;
        for (int i = 0; i < 3; ++i) {
            for (double a = 0.0; a < 105.0; a += 10.0) {
                double ap = Math.min(a, 90.0);
                double sa2 = Math.sin(ap * halfRadFactor);
                double ca2 = Math.cos(ap * halfRadFactor);
                double v[] = {0.0, 0.0, 0.0};
                v[i] = sa2;
                Quat4d quat = new Quat4d(v[0], v[1], v[2], ca2);
                Node.quatToEuler(quat, euler);
                double expected_i = ap;
                double expected_i_1 = 0.0;
                double expected_i_n_1 = 0.0;
                double vE[] = new double[3];
                euler.get(vE);
                assertEquals(expected_i, vE[i], epsilon);
                assertEquals(expected_i_1, vE[(i + 1) % 3], epsilon);
                assertEquals(expected_i_n_1, vE[(i + 2) % 3], epsilon);
            }
        }

        // rotation sequence consistency
        Matrix3d tmpMat = new Matrix3d();
        tmpMat.setIdentity();
        Matrix3d refMat = new Matrix3d();
        refMat.setIdentity();
        Vector3d expected = new Vector3d(90.0, 22.5, 45.0);
        tmpMat.set(new AxisAngle4d(new Vector3d(0.0, 1.0, 0.0), expected.getY() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        tmpMat.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), expected.getZ() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        tmpMat.set(new AxisAngle4d(new Vector3d(1.0, 0.0, 0.0), expected.getX() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        Quat4d q = new Quat4d();
        q.set(refMat);
        q.normalize();
        Node.quatToEuler(q, euler);
        assertTrue(expected.epsilonEquals(euler, epsilon));
    }

    @Test
    public void testNodeEulerToQuat() throws Exception {
        final double epsilon = 0.02;

        // different degrees around different single axis
        final double halfRadFactor = Math.PI / 360.0;
        for (int i = 0; i < 3; ++i) {
            for (double a = 0.0; a < 105.0; a += 10.0) {
                double vE[] = {0.0, 0.0, 0.0};
                vE[i] = a;
                Vector3d v = new Vector3d(vE);
                Quat4d q = new Quat4d();
                Node.eulerToQuat(v, q);
                double qE[] = {0.0, 0.0, 0.0};
                qE[i] = Math.sin(a * halfRadFactor);
                Quat4d expected = new Quat4d(qE[0], qE[1], qE[2], Math.cos(a * halfRadFactor));
                assertTrue(expected.epsilonEquals(q, epsilon));
            }
        }

        // rotation sequence consistency
        Matrix3d tmpMat = new Matrix3d();
        tmpMat.setIdentity();
        Matrix3d refMat = new Matrix3d();
        refMat.setIdentity();
        Vector3d expected = new Vector3d(90.0, 22.5, 45.0);
        tmpMat.set(new AxisAngle4d(new Vector3d(0.0, 1.0, 0.0), expected.getY() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        tmpMat.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), expected.getZ() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        tmpMat.set(new AxisAngle4d(new Vector3d(1.0, 0.0, 0.0), expected.getX() * Math.PI / 180.0));
        refMat.mul(tmpMat);
        Quat4d q = new Quat4d();
        q.set(refMat);
        Quat4d quat = new Quat4d();
        Node.eulerToQuat(expected, quat);
        assertTrue(quat.epsilonEquals(q, epsilon));
    }

}
