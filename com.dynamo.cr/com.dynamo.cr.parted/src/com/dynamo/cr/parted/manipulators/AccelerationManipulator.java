package com.dynamo.cr.parted.manipulators;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.parted.nodes.AccelerationNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class AccelerationManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private boolean sizeChanged = false;
    private double magnitude;
    private double originalMagnitude;

    public AccelerationManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(xScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof AccelerationNode) {
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
            if (node instanceof AccelerationNode) {
                AccelerationNode modifier = (AccelerationNode) node;
                if (manipulator == xScaleManipulator) {
                    magnitude = originalMagnitude + xScaleManipulator.getDistance() * factor;
                    modifier.setMagnitude(magnitude);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        AccelerationNode modifier = (AccelerationNode) getSelection().get(0);
        magnitude = originalMagnitude = modifier.getMagnitude();
        setTranslation(modifier.getTranslation());
        setRotation(modifier.getRotation());
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            AccelerationNode modifier = (AccelerationNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            modifier.setMagnitude(originalMagnitude);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (magnitude != originalMagnitude) {
                operation = propertyModel.setPropertyValue("magnitude", magnitude);
            }
            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalMagnitude = magnitude;
        }
    }

}
