package com.dynamo.cr.ddfeditor.test;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.BoxCollisionShapeNode;
import com.dynamo.cr.ddfeditor.scene.CapsuleCollisionShapeNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectLoader;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.Messages;
import com.dynamo.cr.ddfeditor.scene.SphereCollisionShapeNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;

public class CollisionObjectNodeTest extends AbstractNodeTest {

    private CollisionObjectNode node;
    private CollisionObjectLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new CollisionObjectLoader();

        this.node = registerAndLoadRoot(CollisionObjectNode.class, "collisionobject", this.loader);

        String ddf = "collision_shape: \"/box.convexshape\" " +
                "type: COLLISION_OBJECT_TYPE_KINEMATIC " +
                "mass: 0.0 " +
                "friction: 0.0 " +
                "restitution: 1.0 " +
                "linear_damping: 0.0 " +
                "angular_damping: 0.0 " +
                "locked_rotation: false " +
                "group: \"2\" " +
                "mask: \"1\" ";
        registerFile("/test.collisionobject", ddf);
    }

    @Test
    public void testLoad() throws Exception {

        when(getLoaderContext().loadNode(anyString())).thenReturn(new BoxCollisionShapeNode(new Vector4d(), new Quat4d(), new float[] {1, 1, 1}, 0));

        this.node = this.loader.load(getLoaderContext(), getFile("/test.collisionobject").getContents());
        assertNotNull(this.node);
        Node shape = this.node.getCollisionShapeNode();
        assertNotNull(shape);
    }

    @Test
    public void testEmbeddedShapes() throws Exception {
        IFile coFile = getFile("/collision_objects/embedded_shapes.collisionobject");
        CollisionObjectNode co = this.loader.load(getLoaderContext(), coFile.getContents());
        assertNotNull(co);

        List<Node> children = co.getChildren();
        assertThat(children.size(), is(3));

        int i = 0;
        assertThat(children.get(i), instanceOf(SphereCollisionShapeNode.class));
        SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) children.get(i);
        assertThat(sphere.getDiameter(), is(16.0));

        ++i;
        assertThat(children.get(i), instanceOf(BoxCollisionShapeNode.class));
        BoxCollisionShapeNode box = (BoxCollisionShapeNode) children.get(i);
        assertThat(box.getWidth(), is(20.0));
        assertThat(box.getHeight(), is(40.0));
        assertThat(box.getDepth(), is(60.0));

        ++i;
        assertThat(children.get(i), instanceOf(CapsuleCollisionShapeNode.class));
        CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) children.get(i);
        assertThat(capsule.getDiameter(), is(246.0));
        assertThat(capsule.getHeight(), is(456.0));
    }

    @Test
    public void testEmbeddedShapesErrors() throws Exception {
        IFile coFile = getFile("/collision_objects/bounds_error.collisionobject");
        CollisionObjectNode co = this.loader.load(getLoaderContext(), coFile.getContents());
        assertNotNull(co);
        getModel().setRoot(co);

        List<Node> children = co.getChildren();
        assertThat(children.size(), is(2));

        int i = 0;
        assertThat(children.get(i), instanceOf(SphereCollisionShapeNode.class));
        SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) children.get(i);
        assertThat(sphere.getDiameter(), is(0.0));
        assertThat(sphere.getStatus().getSeverity(), is(IStatus.ERROR));
        assertNodeStatus(sphere, IStatus.ERROR, Messages.CollisionShape_bounds_ERROR);
        assertNodeStatus(sphere, IStatus.ERROR, com.dynamo.cr.properties.Messages.GreaterThanZero_OUTSIDE_RANGE);

        // Set diameter to valid value. The shape should now be valid
        setNodeProperty(sphere, "diameter", 2.0);
        assertThat(sphere.getStatus().getSeverity(), is(IStatus.OK));

        ++i;
        assertThat(children.get(i), instanceOf(BoxCollisionShapeNode.class));
        BoxCollisionShapeNode box = (BoxCollisionShapeNode) children.get(i);
        assertThat(box.getWidth(), is(0.0));
        assertThat(box.getHeight(), is(0.0));
        assertThat(box.getDepth(), is(0.0));
        assertThat(box.getStatus().getSeverity(), is(IStatus.ERROR));
        assertNodeStatus(box, IStatus.ERROR, Messages.CollisionShape_bounds_ERROR);

        // Set box width to valid value. Bounds error should be cleared but height and depth are still equal to zero
        setNodeProperty(box, "width", 1.0);
        assertThat(box.getStatus().getSeverity(), is(IStatus.ERROR));
        assertNodeStatus(box, IStatus.ERROR, com.dynamo.cr.properties.Messages.GreaterThanZero_OUTSIDE_RANGE);

        setNodeProperty(box, "height", 1.0);
        setNodeProperty(box, "depth", 1.0);
        assertThat(box.getStatus().getSeverity(), is(IStatus.OK));
    }

    @Test
    public void testLoadSaveReferenceShape() throws Exception {
        String path = "/test.collisionobject";
        saveLoadCompare(this.loader, CollisionObjectDesc.newBuilder(), path);
    }

    @Test
    public void testLoadSaveEmbeddedShape() throws Exception {
        String path = "/collision_objects/embedded_shapes.collisionobject";
        saveLoadCompare(this.loader, CollisionObjectDesc.newBuilder(), path);
    }

}

