package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.ddfeditor.operations.AddShapeNodeOperation;
import com.dynamo.cr.ddfeditor.operations.RemoveShapeNodeOperation;
import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;


public class CollisionObjectPresenter implements ISceneView.INodePresenter<CollisionObjectNode> {

    public CollisionObjectPresenter() {
    }

    private CollisionObjectNode findCollisionObjectFromSelection(IStructuredSelection selection) {
        Object[] nodes = selection.toArray();
        CollisionObjectNode parent = null;
        for (Object node : nodes) {
            if (node instanceof CollisionObjectNode) {
                parent = (CollisionObjectNode)node;
                break;
            } else if (node instanceof ComponentNode) {
                parent = (CollisionObjectNode)((Node)node).getParent();
                break;
            }
        }
        return parent;
    }

    public void onAddShape(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        // TODO: Support multi selection?
        CollisionObjectNode parent = findCollisionObjectFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collision object in selection.");
        }
        String shapeType = presenterContext.selectFromList("Add Shape", "Select shape type:", "Sphere", "Box", "Capsule");
        if (shapeType != null) {
            Vector4d position = new Vector4d();
            Quat4d rotation = new Quat4d(0,0,0,1);

            CollisionShapeNode shapeNode = null;

            if (shapeType.equals("Sphere")) {
                shapeNode = new SphereCollisionShapeNode(position, rotation, new float[] {1}, 0);
            } else if (shapeType.equals("Box")) {
                shapeNode = new BoxCollisionShapeNode(position, rotation, new float[] {1,1,1}, 0);
            } else if (shapeType.equals("Capsule")) {
                shapeNode = new CapsuleCollisionShapeNode(position, rotation, new float[] {0.5f, 1}, 0);
            } else {
                throw new RuntimeException("Unknown type: " + shapeType);
            }

            presenterContext.executeOperation(new AddShapeNodeOperation(parent, shapeNode));
        }
    }

    public void onRemoveShape(IPresenterContext presenterContext,
            ILoaderContext loaderContext) {
        // TODO: Support multi selection?
        IStructuredSelection structuredSelection = presenterContext.getSelection();
        Object[] nodes = structuredSelection.toArray();
        CollisionShapeNode component = null;
        for (Object node : nodes) {
            if (node instanceof CollisionShapeNode) {
                component = (CollisionShapeNode)node;
                break;
            }
        }
        presenterContext.executeOperation(new RemoveShapeNodeOperation(component));

    }

}
