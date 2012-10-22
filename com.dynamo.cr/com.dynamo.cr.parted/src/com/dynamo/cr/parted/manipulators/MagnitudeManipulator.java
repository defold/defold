package com.dynamo.cr.parted.manipulators;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.parted.nodes.AbstractModifierNode;
import com.dynamo.cr.parted.nodes.DragNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class MagnitudeManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private boolean sizeChanged = false;
    private double magnitude;
    private double originalMagnitude;

    public MagnitudeManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(xScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && (selection[0] instanceof AbstractModifierNode || selection[0] instanceof DragNode)) {
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
            if (node instanceof AbstractModifierNode) {
                AbstractModifierNode modifier = (AbstractModifierNode) node;
                if (manipulator == xScaleManipulator) {
                    magnitude = originalMagnitude + xScaleManipulator.getDistance() * factor;
                    ValueSpread magnitudeVS = modifier.getMagnitude();
                    magnitudeVS.setValue(magnitude);
                    modifier.setMagnitude(magnitudeVS);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        AbstractModifierNode modifier = (AbstractModifierNode) getSelection().get(0);
        magnitude = originalMagnitude = modifier.getMagnitude().getValue();
        setTranslation(modifier.getTranslation());
        setRotation(modifier.getRotation());
    }

    @Override
    public void refresh() {
        AbstractModifierNode node = (AbstractModifierNode) getSelection().get(0);
        ValueSpread vs = node.getMagnitude();
        this.xScaleManipulator.setEnabled(!vs.isAnimated());

        selectionChanged();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            AbstractModifierNode modifier = (AbstractModifierNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            ValueSpread magnitudeVS = modifier.getMagnitude();
            magnitudeVS.setValue(originalMagnitude);
            modifier.setMagnitude(magnitudeVS);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (magnitude != originalMagnitude) {
                magnitudeVS.setValue(magnitude);
                operation = propertyModel.setPropertyValue("magnitude", magnitudeVS);
            }
            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalMagnitude = magnitude;
        }
    }

}
