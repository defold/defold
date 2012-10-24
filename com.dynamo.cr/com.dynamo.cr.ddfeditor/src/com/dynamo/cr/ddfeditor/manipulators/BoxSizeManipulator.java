package com.dynamo.cr.ddfeditor.manipulators;

import javax.vecmath.Quat4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.ddfeditor.scene.BoxCollisionShapeNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class BoxSizeManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private ScaleAxisManipulator yScaleManipulator;
    private ScaleAxisManipulator zScaleManipulator;
    private boolean sizeChanged = false;
    private double width, height, depth;
    private double originalWidth, originalHeight, originalDepth;

    public BoxSizeManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        yScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 1, 0, 1});
        zScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 0, 1, 1});

        yScaleManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));
        zScaleManipulator.setRotation(new Quat4d(-0.5, -0.5, -0.5, 0.5));

        addChild(xScaleManipulator);
        addChild(yScaleManipulator);
        addChild(zScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof BoxCollisionShapeNode) {
            return true;
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        sizeChanged = true;
        double factor = 2.0;
        for (Node node : getSelection()) {
            if (node instanceof BoxCollisionShapeNode) {
                BoxCollisionShapeNode box = (BoxCollisionShapeNode) node;
                if (manipulator == xScaleManipulator) {
                    width = originalWidth + xScaleManipulator.getDistance() * factor;
                    width = Math.max(0, width);
                    box.setWidth(width);
                } else if (manipulator == yScaleManipulator) {
                    height = originalHeight + yScaleManipulator.getDistance() * factor;
                    height = Math.max(0, height);
                    box.setHeight(height);
                } else if (manipulator == zScaleManipulator) {
                    depth = originalDepth + zScaleManipulator.getDistance() * factor;
                    depth = Math.max(0, depth);
                    box.setDepth(depth);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        BoxCollisionShapeNode node = (BoxCollisionShapeNode) getSelection().get(0);
        width = originalWidth = node.getWidth();
        height = originalHeight = node.getHeight();
        depth = originalDepth = node.getDepth();
        setTranslation(node.getTranslation());
        setRotation(node.getRotation());
    }

    @Override
    public void refresh() {
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            BoxCollisionShapeNode box = (BoxCollisionShapeNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            box.setWidth(originalWidth);
            box.setHeight(originalHeight);
            box.setDepth(originalDepth);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (width != originalWidth) {
                operation = propertyModel.setPropertyValue("width", width);
            } else if (height != originalHeight) {
                operation = propertyModel.setPropertyValue("height", height);
            } else if (depth != originalDepth) {
                operation = propertyModel.setPropertyValue("depth", depth);
            }

            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalWidth = width;
            originalHeight = height;
            originalDepth = depth;
        }
    }

}
