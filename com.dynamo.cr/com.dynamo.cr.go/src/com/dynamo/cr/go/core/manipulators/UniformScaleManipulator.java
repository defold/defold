package com.dynamo.cr.go.core.manipulators;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class UniformScaleManipulator extends RootManipulator {

    private ScaleAxisManipulator scaleManipulator;
    private double originalScale;
    private boolean scaleChanged = false;
    private double scale;

    public UniformScaleManipulator() {
        scaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(scaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof InstanceNode) {
            return true;
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        scaleChanged = true;
        double factor = 0.001;
        for (Node node : getSelection()) {
            if (node instanceof InstanceNode) {
                InstanceNode instance = (InstanceNode) node;
                scale = this.originalScale + scaleManipulator.getDistance() * factor;
                if (scale == 0.0) {
                    scale = 0.0001;
                }
                instance.setScale(scale);
            }
        }
    }

    @Override
    protected void selectionChanged() {
        Node node = getSelection().get(0);
        originalScale = ((InstanceNode) node).getScale();
        setTranslation(node.getTranslation());
        setRotation(node.getRotation());
    }

    @Override
    public void refresh() {
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (scaleChanged) {
            Node node = getSelection().get(0);
            InstanceNode instance = (InstanceNode) node;
            // We must reset scale. Otherwise setPropertyValue will detect no change and return null operation
            instance.setScale(originalScale);
            // Set original scale to current in order to "continue" scaling
            originalScale = scale;

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);
            IUndoableOperation operation = propertyModel.setPropertyValue("scale", scale);
            if (operation != null) {
                getController().executeOperation(operation);
            }
        }
    }

}
