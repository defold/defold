package com.dynamo.cr.parted.manipulators;

import javax.vecmath.Quat4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.parted.nodes.PositionalModifierNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class PositionalManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private ScaleAxisManipulator yScaleManipulator;
    private boolean sizeChanged = false;
    private double magnitude, maxDistance;
    private double originalMagnitude, originalMaxDistance;

    public PositionalManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        yScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 1, 0, 1});

        yScaleManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));

        addChild(xScaleManipulator);
        addChild(yScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof PositionalModifierNode) {
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
            if (node instanceof PositionalModifierNode) {
                PositionalModifierNode modifier = (PositionalModifierNode) node;
                if (manipulator == xScaleManipulator) {
                    magnitude = originalMagnitude + xScaleManipulator.getDistance() * factor;
                    ValueSpread magnitudeVS = modifier.getMagnitude();
                    magnitudeVS.setValue(magnitude);
                    modifier.setMagnitude(magnitudeVS);
                } else if (manipulator == yScaleManipulator) {
                    maxDistance = originalMaxDistance + yScaleManipulator.getDistance() * factor;
                    maxDistance = Math.max(0, maxDistance);
                    modifier.setMaxDistance(maxDistance);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        PositionalModifierNode modifier = (PositionalModifierNode) getSelection().get(0);
        magnitude = originalMagnitude = modifier.getMagnitude().getValue();
        maxDistance = originalMaxDistance = modifier.getMaxDistance();
        setTranslation(modifier.getTranslation());
        setRotation(modifier.getRotation());
    }

    @Override
    public void refresh() {
        PositionalModifierNode node = (PositionalModifierNode) getSelection().get(0);
        ValueSpread vs = node.getMagnitude();
        this.xScaleManipulator.setEnabled(!vs.isAnimated());
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            PositionalModifierNode modifier = (PositionalModifierNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            ValueSpread magnitudeVS = modifier.getMagnitude();
            magnitudeVS.setValue(originalMagnitude);
            modifier.setMagnitude(magnitudeVS);
            modifier.setMaxDistance(originalMaxDistance);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (magnitude != originalMagnitude) {
                magnitudeVS.setValue(magnitude);
                operation = propertyModel.setPropertyValue("magnitude", magnitudeVS);
            } else if (maxDistance != originalMaxDistance) {
                operation = propertyModel.setPropertyValue("maxDistance", maxDistance);
            }
            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalMagnitude = magnitude;
            originalMaxDistance = maxDistance;
        }
    }

}
