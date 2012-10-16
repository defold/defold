package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.Collection;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.CircleManipulator;
import com.dynamo.cr.sceneed.ui.RootManipulator;

@RunWith(Parameterized.class)
public class TransformManipulatorTest extends AbstractManipulatorTest {

    private static final double EPSILON = 0.000001;
    private static final Vector3d DRAG_DELTA = new Vector3d(10.0, 0.0, 0.0);

    private static Matrix4d moveTarget;
    private static Matrix4d rotateTarget;

    static {
        moveTarget = new Matrix4d();
        moveTarget.set(DRAG_DELTA);

        rotateTarget = new Matrix4d();
        rotateTarget.set(new AxisAngle4d(new Vector3d(1.0, 0.0, 0.0), DRAG_DELTA.getX() * CircleManipulator.ROTATE_FACTOR));
    }

    @Parameters
    public static Collection<Object[]> data() {
            return Arrays.asList(new Object[][] {
                    {Activator.MOVE_MODE_ID, moveTarget},
                    {Activator.ROTATE_MODE_ID, rotateTarget}
            });
    }

    private String mode;
    private Matrix4d targetTransform;

    public TransformManipulatorTest(String mode, Matrix4d targetTransform) {
        this.mode = mode;
        this.targetTransform = targetTransform;
    }

    private static void moveNode(Node node, Point3d worldPosition) {
        Matrix4d invWorld = new Matrix4d();
        node.getWorldTransform(invWorld);
        invWorld.invert();
        Point3d localPosition = new Point3d(worldPosition);
        invWorld.transform(localPosition);
        node.setTranslation(localPosition);
    }

    /**
     * Create a sphere which is the child of a node that is translated to (10,10) and rotated 90 deg around z.
     * The sphere has the inverse local transform so it's world transform is the identity transform.
     * @return
     */
    private Node createSphere() {
        DummyNode parent = new DummyNode();
        DummySphere sphere = new DummySphere();
        parent.addChild(sphere);
        Point3d translation = new Point3d(10.0, 10.0, 0.0);
        parent.setTranslation(translation);
        Quat4d rotation = new Quat4d();
        rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), Math.PI * 0.5));
        parent.setRotation(rotation);
        Matrix4d invTransform = new Matrix4d();
        invTransform.set(rotation);
        Matrix4d trans = new Matrix4d();
        trans.set(new Vector3d(translation));
        invTransform.mul(trans, invTransform);
        invTransform.invert();
        sphere.setLocalTransform(invTransform);
        return sphere;
    }

    private static void assertEqualsWorld(Node n1, Matrix4d world) {
        Matrix4d w1 = new Matrix4d();
        n1.getWorldTransform(w1);
        assertTrue(w1.epsilonEquals(world, EPSILON));
    }

    private static void assertEqualsWorld(Node n1, Node n2) {
        Matrix4d w2 = new Matrix4d();
        n2.getWorldTransform(w2);
        assertEqualsWorld(n1, w2);
    }

    /**
     * Basic internal test assumptions.
     * @throws Exception
     */
    @Test
    public void testSetup() throws Exception {
        Node n1 = createSphere();
        Node n2 = new DummySphere();
        assertEqualsWorld(n1, n2);
    }

    @Test
    public void testDrag() throws Exception {
        setManipulatorMode(this.mode);

        Node sphere = createSphere();
        select(sphere);

        dragHold(handle(0), DRAG_DELTA);

        assertEqualsWorld(sphere, this.targetTransform);
    }

    @Test
    public void testFollow() throws Exception {
        setManipulatorMode(this.mode);

        Node sphere = createSphere();
        select(sphere);

        RootManipulator rootManipulator = getRootManipulator();

        // Precondition
        assertEqualsWorld(sphere, rootManipulator);

        // Change node position
        moveNode(sphere, new Point3d(10, 0, 0));
        refreshController();

        // Ensure that the manipulator follows..
        assertEqualsWorld(sphere, rootManipulator);
    }

    @Test
    public void testFollowMulti() throws Exception {
        setManipulatorMode(this.mode);

        Node sphere = createSphere();
        select(sphere, new DummySphere());

        RootManipulator rootManipulator = getRootManipulator();

        // Precondition
        assertEqualsWorld(sphere, rootManipulator);

        // Change node position
        moveNode(sphere, new Point3d(10, 0, 0));
        refreshController();

        // Ensure that the manipulator follows..
        assertEqualsWorld(sphere, rootManipulator);
    }

    @Test
    public void testOperation() throws Exception {
        setManipulatorMode(this.mode);

        Node sphere = createSphere();
        select(sphere);

        assertNoCommand();

        dragRelease(handle(0), new Vector3d(10.0, 0.0, 0.0));

        // Verify that a operation was executed
        assertCommand();
    }

    @Test
    public void testNop() throws Exception {
        setManipulatorMode(this.mode);

        Node sphere = createSphere();
        select(sphere);

        assertNoCommand();

        click(handle(0));

        assertNoCommand();
    }
}

