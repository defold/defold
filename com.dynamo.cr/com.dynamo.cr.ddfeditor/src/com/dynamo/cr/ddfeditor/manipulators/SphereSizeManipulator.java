package com.dynamo.cr.ddfeditor.manipulators;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.ddfeditor.scene.SphereCollisionShapeNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

@SuppressWarnings("serial")
public class SphereSizeManipulator extends RootManipulator {

    private ScaleAxisManipulator scaleManipulator;
    private double originalDiameter;
    private boolean diameterChanged = false;
    private double diameter;

    public SphereSizeManipulator() {
        scaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(scaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof SphereCollisionShapeNode) {
            return true;
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        diameterChanged = true;
        double factor = 2.0;
        for (Node node : getSelection()) {
            if (node instanceof SphereCollisionShapeNode) {
                SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) node;
                diameter = originalDiameter + scaleManipulator.getDistance() * factor;
                diameter = Math.max(0, diameter);
                sphere.setDiameter(diameter);
            }
        }
    }

    @Override
    protected void selectionChanged() {
        Node node = getSelection().get(0);
        originalDiameter = ((SphereCollisionShapeNode) node).getDiameter();
        setTranslation(node.getTranslation());
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (diameterChanged) {
            Node node = getSelection().get(0);
            SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) node;
            // We must reset diameter. Otherwise setPropertyValue will detect no change and return null operation
            sphere.setDiameter(originalDiameter);
            // Set original diameter to current in order to "continue" scaling
            originalDiameter = diameter;

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);
            IUndoableOperation operation = propertyModel.setPropertyValue("diameter", diameter);
            if (operation != null) {
                getController().executeOperation(operation);
            }
        }
    }

}
