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
    private double diameter, height;
    private double originalDiameter, originalHeight;

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
        double factor = 2.0;
        for (Node node : getSelection()) {
            if (node instanceof CapsuleCollisionShapeNode) {
                CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) node;
                if (manipulator == xScaleManipulator) {
                    diameter = originalDiameter + xScaleManipulator.getDistance() * factor;
                    diameter = Math.max(0, diameter);
                    capsule.setDiameter(diameter);
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
        diameter = originalDiameter = node.getDiameter();
        height = originalHeight = node.getHeight();
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
            CapsuleCollisionShapeNode capsule = (CapsuleCollisionShapeNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            capsule.setDiameter(originalDiameter);
            capsule.setHeight(originalHeight);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (diameter != originalDiameter) {
                operation = propertyModel.setPropertyValue("diameter", diameter);
            } else if (height != originalHeight) {
                operation = propertyModel.setPropertyValue("height", height);
            }

            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalDiameter = diameter;
            originalHeight = height;
        }
    }

}
