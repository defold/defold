package com.dynamo.cr.integrationtest;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.osgi.util.NLS;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.BoxCollisionShapeNode;
import com.dynamo.cr.ddfeditor.scene.CapsuleCollisionShapeNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.Messages;
import com.dynamo.cr.ddfeditor.scene.SphereCollisionShapeNode;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class CollisionObjectTest extends AbstractNodeTest {

    @Test
    public void testCollisionObject() throws Exception {
        IFile coFile = project.getFile("collision_object/kinematic_box.collisionobject");
        CollisionObjectNode co = (CollisionObjectNode) this.loaderContext.loadNode("collisionobject", coFile.getContents());
        assertNotNull(co);
        Node shape = co.getCollisionShapeNode();
        assertNotNull(shape);
    }

    @Test
    public void testEmbeddedShapes() throws Exception {
        IFile coFile = project.getFile("collision_object/embedded_shapes.collisionobject");
        CollisionObjectNode co = (CollisionObjectNode) this.loaderContext.loadNode("collisionobject", coFile.getContents());
        assertNotNull(co);

        List<Node> children = co.getChildren();
        assertThat(children.size(), is(3));

        int i = 0;
        assertThat(children.get(i), instanceOf(SphereCollisionShapeNode.class));
        SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) children.get(i);
        assertThat(sphere.getRadius(), is(8.0));

        ++i;
        assertThat(children.get(i), instanceOf(BoxCollisionShapeNode.class));
        BoxCollisionShapeNode box = (BoxCollisionShapeNode) children.get(i);
        assertThat(box.getWidth(), is(10.0));
        assertThat(box.getHeight(), is(20.0));
        assertThat(box.getDepth(), is(30.0));

        ++i;
        assertThat(children.get(i), instanceOf(CapsuleCollisionShapeNode.class));
        CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) children.get(i);
        assertThat(capsule.getRadius(), is(123.0));
        assertThat(capsule.getHeight(), is(456.0));
    }

    @Test
    public void testEmbeddedShapesErrors() throws Exception {
        IFile coFile = project.getFile("collision_object/bounds_error.collisionobject");
        CollisionObjectNode co = (CollisionObjectNode) this.loaderContext.loadNode("collisionobject", coFile.getContents());
        assertNotNull(co);

        List<Node> children = co.getChildren();
        assertThat(children.size(), is(2));

        int i = 0;
        assertThat(children.get(i), instanceOf(SphereCollisionShapeNode.class));
        SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) children.get(i);
        assertThat(sphere.getRadius(), is(0.0));
        assertThat(sphere.validate().getSeverity(), is(IStatus.ERROR));
        assertMessageExists(sphere.validate(), NLS.bind(Messages.CollisionShape_bounds_ERROR, null));
        assertMessageExists(sphere.validate(), NLS.bind(com.dynamo.cr.properties.Messages.GreaterThanZero_OUTSIDE_RANGE, null));

        // Set radius to valid value. The shape should now be valid
        sphere.setRadius(1.0);
        assertThat(sphere.validate().getSeverity(), is(IStatus.OK));

        ++i;
        assertThat(children.get(i), instanceOf(BoxCollisionShapeNode.class));
        BoxCollisionShapeNode box = (BoxCollisionShapeNode) children.get(i);
        assertThat(box.getWidth(), is(0.0));
        assertThat(box.getHeight(), is(0.0));
        assertThat(box.getDepth(), is(0.0));
        assertThat(box.validate().getSeverity(), is(IStatus.ERROR));
        assertMessageExists(box.validate(), NLS.bind(Messages.CollisionShape_bounds_ERROR, null));

        // Set box width to valid value. Bounds error should be cleared but height and depth are still equal to zero
        box.setWidth(1);
        assertThat(box.validate().getSeverity(), is(IStatus.ERROR));
        assertMessageExists(box.validate(), NLS.bind(com.dynamo.cr.properties.Messages.GreaterThanZero_OUTSIDE_RANGE, null));

        box.setHeight(1);
        box.setDepth(1);
        assertThat(box.validate().getSeverity(), is(IStatus.OK));
    }

    @SuppressWarnings("rawtypes")
    <T extends Node> void saveLoadCompare(ILoaderContext context, GeneratedMessage.Builder builder, Class<T> klass, IFile file) throws IOException, CoreException {

        Reader reader = new InputStreamReader(file.getContents());
        TextFormat.merge(reader, builder);
        reader.close();
        Message directMessage = builder.build();

        INodeTypeRegistry registry = this.loaderContext.getNodeTypeRegistry();
        INodeType nodeType = registry.getNodeType(klass);
        INodeLoader<Node> loader = nodeType.getLoader();

        @SuppressWarnings("unchecked")
        T node = (T) loader.load(context, file.getContents());
        Message createdMessage = loader.buildMessage(context, node, new NullProgressMonitor());
        assertEquals(directMessage.toString(), createdMessage.toString());
    }

    @Test
    public void testLoadSaveReferenceShape() throws Exception {
        IFile coFile = project.getFile("collision_object/kinematic_box.collisionobject");
        saveLoadCompare(this.loaderContext, CollisionObjectDesc.newBuilder(), CollisionObjectNode.class, coFile);
    }

    @Test
    public void testLoadSaveEmbeddedShape() throws Exception {
        IFile coFile = project.getFile("collision_object/embedded_shapes.collisionobject");
        saveLoadCompare(this.loaderContext, CollisionObjectDesc.newBuilder(), CollisionObjectNode.class, coFile);
    }

    /*
    We "repair" invalid data so we can't run a test like this
    @Test
    public void testLoadSaveInvaliedEmbeddedShape() throws Exception {
        IFile coFile = project.getFile("collision_object/bounds_error.collisionobject");
        saveLoadCompare(this.loaderContext, CollisionObjectDesc.newBuilder(), CollisionObjectNode.class, coFile);
    }
    */
}

