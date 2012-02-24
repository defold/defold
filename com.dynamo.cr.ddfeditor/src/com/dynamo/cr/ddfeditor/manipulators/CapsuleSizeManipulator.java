package com.dynamo.cr.ddfeditor.manipulators;

import javax.vecmath.Quat4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.ddfeditor.scene.CapsuleCollisionShapeNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class CapsuleSizeManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private ScaleAxisManipulator yScaleManipulator;
    private boolean sizeChanged = false;
    private double radius, height;
    private double originalRadius, originalHeight;

    public CapsuleSizeManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        yScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 1, 0, 1});

        yScaleManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));

        addChild(xScaleManipulator);
        addChild(yScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof CapsuleCollisionShapeNode) {
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
        double factor = 0.5;
        for (Node node : getSelection()) {
            if (node instanceof CapsuleCollisionShapeNode) {
                CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) node;
                if (manipulator == xScaleManipulator) {
                    radius = originalRadius + xScaleManipulator.getDistance() * factor;
                    radius = Math.max(0, radius);
                    capsule.setRadius(radius);
                } else if (manipulator == yScaleManipulator) {
                    height = originalHeight + yScaleManipulator.getDistance() * factor;
                    height = Math.max(0, height);
                    capsule.setHeight(height);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        CapsuleCollisionShapeNode node = (CapsuleCollisionShapeNode) getSelection().get(0);
        radius = originalRadius = node.getRadius();
        height = originalHeight = node.getHeight();
        setTranslation(node.getTranslation());
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            capsule.setRadius(originalRadius);
            capsule.setHeight(originalHeight);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (radius != originalRadius) {
                operation = propertyModel.setPropertyValue("radius", radius);
            } else if (height != originalHeight) {
                operation = propertyModel.setPropertyValue("height", height);
            }

            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalRadius = radius;
            originalHeight = height;
        }
    }

}
